// EchoService.cpp : Windows service mở TCP cổng 9999, echo lại mọi byte nhận được.
//
// Cách dùng (chạy cmd/PowerShell bằng quyền Administrator):
//   EchoService.exe install     Cài service
//   EchoService.exe start       Chạy service
//   EchoService.exe stop        Dừng service
//   EchoService.exe uninstall   Dừng + gỡ service
//   EchoService.exe console     Chạy thử trên console (Ctrl+C để dừng)
//
// Không treo khi stop: luồng làm việc chỉ chờ trên WaitForMultipleObjects
// (event dừng + event của các socket, socket ở chế độ non-blocking qua WSAEventSelect).
// Khi SCM gửi SERVICE_CONTROL_STOP, handler chỉ SetEvent -> vòng lặp thoát ngay,
// đóng toàn bộ socket rồi báo SERVICE_STOPPED.

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Advapi32.lib")

const WCHAR SERVICE_NAME[] = L"ChatEchoService";
const WCHAR SERVICE_DISPLAY[] = L"Chat Echo Service";
const WCHAR SERVICE_DESC[] = L"Mở TCP cổng 9999 và gửi lại (echo) mọi dữ liệu nhận được cho client.";
const u_short ECHO_PORT = 9999;
const int IO_CHUNK = 64 * 1024;
const SIZE_T MAX_PENDING = 16 * 1024 * 1024;		// Giới hạn dữ liệu đọc nốt khi client đóng kết nối
const DWORD MAX_CLIENTS = WSA_MAXIMUM_WAIT_EVENTS - 2;	// Trừ event dừng và event của socket lắng nghe

// Bộ đệm byte động cấp phát bằng Heap API (giống tuần 4)
struct ByteBuffer
{
	BYTE* data = nullptr;
	SIZE_T size = 0;
	SIZE_T capacity = 0;
};

struct Client
{
	SOCKET socket = INVALID_SOCKET;
	WSAEVENT event = WSA_INVALID_EVENT;
	ByteBuffer pending;		// Dữ liệu echo chưa gửi được (send trả WSAEWOULDBLOCK)
	SIZE_T sent = 0;		// Số byte của pending đã gửi
	bool closing = false;	// Client đã đóng chiều gửi, chỉ còn chờ gửi nốt pending
};

static SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
static SERVICE_STATUS g_status = {};
static HANDLE g_stopEvent = nullptr;
static bool g_console = false;

static Client g_clients[MAX_CLIENTS];
static DWORD g_clientCount = 0;

// ---------------------------------------------------------------------------
// Tiện ích

static void Log(LPCWSTR format, ...)
{
	WCHAR text[1024];
	va_list args;
	va_start(args, format);
	// https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-wvsprintfw
	int length = wvsprintfW(text, format, args);
	va_end(args);
	if (length < 0)
		return;
	if (length > (int)ARRAYSIZE(text) - 3)
		length = (int)ARRAYSIZE(text) - 3;
	text[length++] = L'\r';
	text[length++] = L'\n';
	text[length] = L'\0';

	if (g_console)
	{
		DWORD written = 0;
		WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), text, (DWORD)length, &written, nullptr);
	}
	else
		OutputDebugStringW(text);	// Xem bằng DebugView khi chạy dưới dạng service
}

static bool BufferReserve(ByteBuffer& buffer, SIZE_T need)
{
	if (need <= buffer.capacity)
		return true;
	SIZE_T capacity = buffer.capacity * 2;
	if (capacity < need)
		capacity = need;
	if (capacity < IO_CHUNK)
		capacity = IO_CHUNK;

	HANDLE heap = GetProcessHeap();
	BYTE* data;
	if (buffer.data == nullptr)
		data = (BYTE*)HeapAlloc(heap, 0, capacity);
	else
		data = (BYTE*)HeapReAlloc(heap, 0, buffer.data, capacity);
	if (data == nullptr)
		return false;
	buffer.data = data;
	buffer.capacity = capacity;
	return true;
}

static bool BufferAppend(ByteBuffer& buffer, const void* source, SIZE_T count)
{
	if (!BufferReserve(buffer, buffer.size + count))
		return false;
	CopyMemory(buffer.data + buffer.size, source, count);
	buffer.size += count;
	return true;
}

static void BufferFree(ByteBuffer& buffer)
{
	if (buffer.data != nullptr)
		HeapFree(GetProcessHeap(), 0, buffer.data);
	buffer.data = nullptr;
	buffer.size = 0;
	buffer.capacity = 0;
}

// ---------------------------------------------------------------------------
// Trạng thái service

static void ReportStatus(DWORD state, DWORD exitCode, DWORD waitHint)
{
	static DWORD checkPoint = 1;
	if (g_console)
		return;

	g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_status.dwCurrentState = state;
	g_status.dwWin32ExitCode = exitCode;
	g_status.dwWaitHint = waitHint;
	// Chỉ nhận lệnh stop khi đang chạy
	g_status.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
	if (state == SERVICE_RUNNING || state == SERVICE_STOPPED)
		g_status.dwCheckPoint = 0;
	else
		g_status.dwCheckPoint = checkPoint++;
	// https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-setservicestatus
	SetServiceStatus(g_statusHandle, &g_status);
}

// Chạy trên luồng của dispatcher: chỉ báo trạng thái và set event, không chờ gì -> không treo
static DWORD WINAPI ServiceCtrlHandler(DWORD control, DWORD, LPVOID, LPVOID)
{
	switch (control)
	{
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
		ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
		SetEvent(g_stopEvent);
		return NO_ERROR;
	case SERVICE_CONTROL_INTERROGATE:
		return NO_ERROR;
	}
	return ERROR_CALL_NOT_IMPLEMENTED;
}

static BOOL WINAPI ConsoleCtrlHandler(DWORD type)
{
	if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT)
	{
		Log(L"Đang dừng...");
		SetEvent(g_stopEvent);
		return TRUE;
	}
	return FALSE;
}

// ---------------------------------------------------------------------------
// Echo server

static void CloseClient(DWORD index)
{
	Client& client = g_clients[index];
	Log(L"Client %u ngắt kết nối.", (unsigned)client.socket);
	closesocket(client.socket);
	WSACloseEvent(client.event);
	BufferFree(client.pending);

	// Dồn phần tử cuối vào chỗ trống để mảng event luôn liên tục
	g_clientCount--;
	if (index != g_clientCount)
		g_clients[index] = g_clients[g_clientCount];
	g_clients[g_clientCount] = Client();
}

// Gửi phần còn lại của pending. Trả về false nếu lỗi (phải đóng client)
static bool FlushClient(Client& client)
{
	while (client.sent < client.pending.size)
	{
		SIZE_T remaining = client.pending.size - client.sent;
		int length = remaining < (SIZE_T)IO_CHUNK ? (int)remaining : IO_CHUNK;
		int count = send(client.socket, (const char*)client.pending.data + client.sent, length, 0);
		if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			return true;		// Chờ FD_WRITE
		if (count <= 0)
			return false;
		client.sent += (SIZE_T)count;
	}
	client.pending.size = 0;
	client.sent = 0;
	return true;
}

// Đọc dữ liệu đang có và gửi lại ngay. Nếu send trả WSAEWOULDBLOCK thì giữ phần thừa trong
// pending và ngừng recv: TCP flow control sẽ tự chặn client gửi tiếp, khi có FD_WRITE
// gửi xong pending mới đọc tiếp. Trả về false nếu phải đóng client
static bool EchoClient(Client& client)
{
	char buffer[8192];
	while (client.pending.size == 0)
	{
		int count = recv(client.socket, buffer, sizeof(buffer), 0);
		if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			return true;
		if (count <= 0)
			return false;		// 0 = client đóng kết nối, SOCKET_ERROR = lỗi

		int done = 0;
		while (done < count)
		{
			int sent = send(client.socket, buffer + done, count - done, 0);
			if (sent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
				break;
			if (sent <= 0)
				return false;
			done += sent;
		}
		if (done < count && !BufferAppend(client.pending, buffer + done, (SIZE_T)(count - done)))
			return false;
	}
	return true;
}

// Client đã đóng chiều gửi (FD_CLOSE): đọc nốt dữ liệu còn lại vào pending để echo nốt
static bool ReadRest(Client& client)
{
	char buffer[8192];
	for (;;)
	{
		int count = recv(client.socket, buffer, sizeof(buffer), 0);
		if (count == 0 || (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK))
			return true;
		if (count < 0)
			return false;
		if (client.pending.size + (SIZE_T)count > MAX_PENDING
			|| !BufferAppend(client.pending, buffer, (SIZE_T)count))
			return false;
	}
}

static void AcceptClients(SOCKET listener)
{
	for (;;)
	{
		sockaddr_in address = {};
		int size = sizeof(address);
		SOCKET socket = accept(listener, (sockaddr*)&address, &size);
		if (socket == INVALID_SOCKET)
			return;		// WSAEWOULDBLOCK: hết kết nối đang chờ

		WCHAR ip[INET_ADDRSTRLEN] = {};
		InetNtopW(AF_INET, &address.sin_addr, ip, ARRAYSIZE(ip));

		if (g_clientCount >= MAX_CLIENTS)
		{
			Log(L"Từ chối %s:%u (quá %u client).", ip, ntohs(address.sin_port), MAX_CLIENTS);
			closesocket(socket);
			continue;
		}

		// Socket accept được kế thừa WSAEventSelect của listener -> đăng ký lại event riêng
		WSAEVENT event = WSACreateEvent();
		if (event == WSA_INVALID_EVENT
			|| WSAEventSelect(socket, event, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
		{
			if (event != WSA_INVALID_EVENT)
				WSACloseEvent(event);
			closesocket(socket);
			continue;
		}

		Client& client = g_clients[g_clientCount++];
		client.socket = socket;
		client.event = event;
		Log(L"Client %u kết nối từ %s:%u.", (unsigned)socket, ip, ntohs(address.sin_port));
	}
}

static void HandleClient(DWORD index)
{
	Client& client = g_clients[index];
	WSANETWORKEVENTS events = {};
	// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsaenumnetworkevents
	if (WSAEnumNetworkEvents(client.socket, client.event, &events) == SOCKET_ERROR)
	{
		CloseClient(index);
		return;
	}
	if (events.lNetworkEvents == 0)
		return;

	bool ok = true;
	if (events.lNetworkEvents & FD_WRITE)
		ok = FlushClient(client);
	if (ok && (events.lNetworkEvents & FD_CLOSE))
	{
		client.closing = true;
		ok = ReadRest(client) && FlushClient(client);
	}
	else if (ok && !client.closing)
		ok = EchoClient(client);	// FD_READ, hoặc vừa gửi xong pending thì đọc tiếp
	// Client đã đóng và đã echo hết thì đóng socket
	if (ok && client.closing && client.pending.size == 0)
		ok = false;
	if (!ok)
		CloseClient(index);
}

// Vòng lặp chính. Trả về mã lỗi Win32 (NO_ERROR nếu dừng bình thường)
static DWORD RunEchoServer()
{
	WSADATA data = {};
	int error = WSAStartup(MAKEWORD(2, 2), &data);
	if (error != 0)
	{
		Log(L"WSAStartup lỗi %d.", error);
		return (DWORD)error;
	}

	DWORD result = NO_ERROR;
	WSAEVENT listenEvent = WSA_INVALID_EVENT;
	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listener == INVALID_SOCKET)
	{
		result = (DWORD)WSAGetLastError();
		Log(L"Không tạo được socket (lỗi %u).", result);
		WSACleanup();
		return result;
	}

	sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_port = htons(ECHO_PORT);
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	listenEvent = WSACreateEvent();
	if (listenEvent == WSA_INVALID_EVENT
		|| bind(listener, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR
		|| listen(listener, SOMAXCONN) == SOCKET_ERROR
		|| WSAEventSelect(listener, listenEvent, FD_ACCEPT) == SOCKET_ERROR)
	{
		result = (DWORD)WSAGetLastError();
		Log(L"Không mở được cổng %u (lỗi %u).", ECHO_PORT, result);
	}
	else
	{
		Log(L"Đang lắng nghe TCP cổng %u.", ECHO_PORT);
		ReportStatus(SERVICE_RUNNING, NO_ERROR, 0);

		HANDLE handles[WSA_MAXIMUM_WAIT_EVENTS];
		for (;;)
		{
			handles[0] = g_stopEvent;
			handles[1] = listenEvent;
			for (DWORD i = 0; i < g_clientCount; i++)
				handles[2 + i] = g_clients[i].event;

			DWORD wait = WaitForMultipleObjects(2 + g_clientCount, handles, FALSE, INFINITE);
			if (wait == WAIT_OBJECT_0)
				break;		// Có lệnh dừng
			if (wait == WAIT_FAILED)
			{
				result = GetLastError();
				Log(L"WaitForMultipleObjects lỗi %u.", result);
				break;
			}

			WSANETWORKEVENTS events = {};
			if (WSAEnumNetworkEvents(listener, listenEvent, &events) == 0 && (events.lNetworkEvents & FD_ACCEPT))
				AcceptClients(listener);

			// Kiểm tra mọi client (WaitForMultipleObjects chỉ trả về event có chỉ số nhỏ nhất).
			// Duyệt ngược vì CloseClient dồn phần tử cuối vào vị trí bị xóa.
			for (DWORD i = g_clientCount; i > 0; i--)
				HandleClient(i - 1);
		}
	}

	while (g_clientCount > 0)
		CloseClient(g_clientCount - 1);
	closesocket(listener);
	if (listenEvent != WSA_INVALID_EVENT)
		WSACloseEvent(listenEvent);
	WSACleanup();
	Log(L"Đã dừng echo server.");
	return result;
}

static void WINAPI ServiceMain(DWORD, LPWSTR*)
{
	// https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-registerservicectrlhandlerexw
	g_statusHandle = RegisterServiceCtrlHandlerExW(SERVICE_NAME, ServiceCtrlHandler, nullptr);
	if (g_statusHandle == nullptr)
		return;

	ReportStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
	g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (g_stopEvent == nullptr)
	{
		ReportStatus(SERVICE_STOPPED, GetLastError(), 0);
		return;
	}

	DWORD result = RunEchoServer();

	CloseHandle(g_stopEvent);
	g_stopEvent = nullptr;
	ReportStatus(SERVICE_STOPPED, result, 0);
}

// ---------------------------------------------------------------------------
// Cài đặt / điều khiển service bằng Service Control Manager

static void PrintLastError(LPCWSTR action)
{
	DWORD error = GetLastError();
	WCHAR* message = nullptr;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, error, 0, (LPWSTR)&message, 0, nullptr);
	Log(L"%s thất bại (lỗi %u): %s", action, error, message != nullptr ? message : L"");
	if (message != nullptr)
		LocalFree(message);
}

// Chờ service về trạng thái mong muốn, tối đa timeout ms
static bool WaitForState(SC_HANDLE service, DWORD state, DWORD timeout)
{
	DWORD start = GetTickCount();
	SERVICE_STATUS status = {};
	while (QueryServiceStatus(service, &status))
	{
		if (status.dwCurrentState == state)
			return true;
		if (GetTickCount() - start > timeout)
			return false;
		Sleep(200);
	}
	return false;
}

static int InstallService()
{
	WCHAR path[MAX_PATH];
	DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH)
	{
		Log(L"Không lấy được đường dẫn file exe.");
		return 1;
	}
	// Đặt đường dẫn trong ngoặc kép để tránh lỗi đường dẫn có dấu cách (unquoted service path)
	WCHAR command[MAX_PATH + 2];
	wsprintfW(command, L"\"%s\"", path);

	SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
	if (manager == nullptr)
	{
		PrintLastError(L"OpenSCManager (cần chạy bằng quyền Administrator)");
		return 1;
	}
	// https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-createservicew
	SC_HANDLE service = CreateServiceW(manager, SERVICE_NAME, SERVICE_DISPLAY, SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
		command, nullptr, nullptr, nullptr, nullptr, nullptr);
	if (service == nullptr)
	{
		PrintLastError(L"CreateService");
		CloseServiceHandle(manager);
		return 1;
	}

	SERVICE_DESCRIPTIONW description = { (LPWSTR)SERVICE_DESC };
	ChangeServiceConfig2W(service, SERVICE_CONFIG_DESCRIPTION, &description);
	Log(L"Đã cài service %s: %s", SERVICE_NAME, command);

	CloseServiceHandle(service);
	CloseServiceHandle(manager);
	return 0;
}

static int StartEchoService()
{
	SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
	if (manager == nullptr)
	{
		PrintLastError(L"OpenSCManager");
		return 1;
	}
	SC_HANDLE service = OpenServiceW(manager, SERVICE_NAME, SERVICE_START | SERVICE_QUERY_STATUS);
	int result = 1;
	if (service == nullptr)
		PrintLastError(L"OpenService");
	else if (!StartServiceW(service, 0, nullptr))
		PrintLastError(L"StartService");
	else if (WaitForState(service, SERVICE_RUNNING, 10000))
	{
		Log(L"Service đang chạy.");
		result = 0;
	}
	else
		Log(L"Service không chuyển sang RUNNING (xem cổng %u có bị chiếm không).", ECHO_PORT);

	if (service != nullptr)
		CloseServiceHandle(service);
	CloseServiceHandle(manager);
	return result;
}

// Gửi lệnh stop nếu service đang chạy rồi chờ STOPPED
static bool StopServiceAndWait(SC_HANDLE service)
{
	SERVICE_STATUS status = {};
	if (!QueryServiceStatus(service, &status))
		return false;
	if (status.dwCurrentState == SERVICE_STOPPED)
		return true;
	if (status.dwCurrentState != SERVICE_STOP_PENDING && !ControlService(service, SERVICE_CONTROL_STOP, &status))
	{
		PrintLastError(L"ControlService(STOP)");
		return false;
	}
	return WaitForState(service, SERVICE_STOPPED, 10000);
}

static int StopEchoService(bool remove)
{
	SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
	if (manager == nullptr)
	{
		PrintLastError(L"OpenSCManager");
		return 1;
	}
	DWORD access = SERVICE_STOP | SERVICE_QUERY_STATUS | (remove ? DELETE : 0);
	SC_HANDLE service = OpenServiceW(manager, SERVICE_NAME, access);
	int result = 1;
	if (service == nullptr)
		PrintLastError(L"OpenService");
	else if (!StopServiceAndWait(service))
		Log(L"Không dừng được service.");
	else if (!remove)
	{
		Log(L"Đã dừng service.");
		result = 0;
	}
	else if (!DeleteService(service))
		PrintLastError(L"DeleteService");
	else
	{
		Log(L"Đã gỡ service %s.", SERVICE_NAME);
		result = 0;
	}

	if (service != nullptr)
		CloseServiceHandle(service);
	CloseServiceHandle(manager);
	return result;
}

static int RunConsole()
{
	g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
	if (g_stopEvent == nullptr)
		return 1;
	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
	Log(L"Chạy chế độ console, Ctrl+C để dừng.");
	DWORD result = RunEchoServer();
	CloseHandle(g_stopEvent);
	return result == NO_ERROR ? 0 : 1;
}

static void PrintUsage()
{
	Log(L"Cách dùng: EchoService.exe [install | uninstall | start | stop | console]");
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc >= 2)
	{
		g_console = true;
		LPCWSTR command = argv[1];
		if (command[0] == L'-' || command[0] == L'/')
			command++;
		if (lstrcmpiW(command, L"install") == 0)
			return InstallService();
		if (lstrcmpiW(command, L"uninstall") == 0)
			return StopEchoService(true);
		if (lstrcmpiW(command, L"start") == 0)
			return StartEchoService();
		if (lstrcmpiW(command, L"stop") == 0)
			return StopEchoService(false);
		if (lstrcmpiW(command, L"console") == 0)
			return RunConsole();
		PrintUsage();
		return 1;
	}

	// Không có tham số: được SCM khởi động
	SERVICE_TABLE_ENTRYW table[] =
	{
		{ (LPWSTR)SERVICE_NAME, ServiceMain },
		{ nullptr, nullptr }
	};
	// https://learn.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-startservicectrldispatcherw
	if (!StartServiceCtrlDispatcherW(table))
	{
		if (GetLastError() == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
		{
			// Chạy trực tiếp từ Explorer/console mà không có tham số
			g_console = true;
			PrintUsage();
		}
		return 1;
	}
	return 0;
}
