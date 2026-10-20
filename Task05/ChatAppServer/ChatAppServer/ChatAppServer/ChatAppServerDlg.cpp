
// ChatAppServerDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "ChatAppServer.h"
#include "ChatAppServerDlg.h"
#include "afxdialogex.h"

const UINT WM_SOCKET = WM_APP + 1;
const DWORD MAX_MESSAGE_BYTES = 0x7FFFFFFE;
const int IO_CHUNK = 64 * 1024;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


static bool BufferReserve(ByteBuffer& buffer, SIZE_T need)
{
	if (need <= buffer.capacity)
		return true;
	SIZE_T capacity = need;
	if (buffer.capacity <= ((SIZE_T)-1) / 2)
		capacity = buffer.capacity * 2;
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
	if (count > ((SIZE_T)-1) - buffer.size || !BufferReserve(buffer, buffer.size + count))
		return false;
	CopyMemory(buffer.data + buffer.size, source, count);
	buffer.size += count;
	return true;
}

static void BufferConsume(ByteBuffer& buffer, SIZE_T count)
{
	if (count >= buffer.size)
	{
		buffer.size = 0;
		return;
	}
	MoveMemory(buffer.data, buffer.data + count, buffer.size - count);
	buffer.size -= count;
}

static void BufferFree(ByteBuffer& buffer)
{
	if (buffer.data != nullptr)
		HeapFree(GetProcessHeap(), 0, buffer.data);
	buffer.data = nullptr;
	buffer.size = 0;
	buffer.capacity = 0;
}


// Autorun: ghi đường dẫn exe vào HKCU\...\Run, Windows sẽ chạy app khi user đăng nhập
const WCHAR RUN_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const WCHAR RUN_VALUE[] = L"ChatAppServer";

// Lệnh autorun = đường dẫn exe hiện tại đặt trong ngoặc kép (đường dẫn có thể chứa dấu cách)
static bool GetAutorunCommand(WCHAR* command, DWORD size)
{
	WCHAR path[MAX_PATH];
	// https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew
	DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
	if (length == 0 || length >= MAX_PATH || length + 3 > size)
		return false;
	wsprintfW(command, L"\"%s\"", path);
	return true;
}

// Đã bật autorun chưa (giá trị trong Run trỏ đúng exe đang chạy)
static bool IsAutorunEnabled()
{
	WCHAR command[MAX_PATH + 2];
	WCHAR value[MAX_PATH + 2];
	DWORD size = sizeof(value);
	if (!GetAutorunCommand(command, _countof(command)))
		return false;
	// https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-reggetvaluew
	if (RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE, RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS)
		return false;
	return lstrcmpiW(value, command) == 0;
}

// Bật/tắt autorun, trả về mã lỗi Win32
static LSTATUS SetAutorun(bool enable)
{
	HKEY key = nullptr;
	// https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regcreatekeyexw
	LSTATUS status = RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
	if (status != ERROR_SUCCESS)
		return status;

	if (enable)
	{
		WCHAR command[MAX_PATH + 2];
		if (!GetAutorunCommand(command, _countof(command)))
			status = ERROR_FILENAME_EXCED_RANGE;
		else
			// https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regsetvalueexw
			status = RegSetValueExW(key, RUN_VALUE, 0, REG_SZ, (const BYTE*)command,
				(DWORD)(lstrlenW(command) + 1) * sizeof(WCHAR));
	}
	else
	{
		// https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regdeletevaluew
		status = RegDeleteValueW(key, RUN_VALUE);
		if (status == ERROR_FILE_NOT_FOUND)
			status = ERROR_SUCCESS;		// Chưa đăng ký thì coi như đã tắt
	}
	RegCloseKey(key);
	return status;
}


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CChatAppServerDlg dialog



CChatAppServerDlg::CChatAppServerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CHATAPPSERVER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CChatAppServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CChatAppServerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_LISTEN, &CChatAppServerDlg::OnBnClickedButtonListen)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CChatAppServerDlg::OnBnClickedButtonSend)
	ON_BN_CLICKED(IDC_CHECK_AUTORUN, &CChatAppServerDlg::OnBnClickedCheckAutorun)
	ON_MESSAGE(WM_SOCKET, &CChatAppServerDlg::OnSocket)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CChatAppServerDlg message handlers

BOOL CChatAppServerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	::SetWindowTextW(m_hWnd, L"Chat TCP - Máy chủ");

	::SetDlgItemTextW(m_hWnd, IDC_EDIT_PORT, L"5000");

	::SendMessageW(::GetDlgItem(m_hWnd, IDC_EDIT_MESSAGE), EM_SETLIMITTEXT, 0, 0);
	::SendMessageW(::GetDlgItem(m_hWnd, IDC_EDIT_HISTORY), EM_SETLIMITTEXT, 0, 0);

	EnableItem(IDC_BUTTON_SEND, FALSE);
	::CheckDlgButton(m_hWnd, IDC_CHECK_AUTORUN, IsAutorunEnabled() ? BST_CHECKED : BST_UNCHECKED);

	WSADATA data = {};
	m_started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
	if (!m_started)
	{
		AddMessage(L"", L"Không khởi tạo được Winsock.");
		EnableItem(IDC_BUTTON_LISTEN, FALSE);
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CChatAppServerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CChatAppServerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CChatAppServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CChatAppServerDlg::EnableItem(int id, BOOL enable)
{
	::EnableWindow(::GetDlgItem(m_hWnd, id), enable);
}

void CChatAppServerDlg::AddMessage(LPCWSTR prefix, LPCWSTR text)
{
	HWND history = ::GetDlgItem(m_hWnd, IDC_EDIT_HISTORY);

	int length = ::GetWindowTextLengthW(history);

	::SendMessageW(history, EM_SETSEL, (WPARAM)length, (LPARAM)length);
	::SendMessageW(history, EM_REPLACESEL, FALSE, (LPARAM)prefix);
	::SendMessageW(history, EM_REPLACESEL, FALSE, (LPARAM)text);
	::SendMessageW(history, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
}

void CChatAppServerDlg::ShowAddresses(UINT port)
{
	WCHAR host[256] = {};
	if (GetHostNameW(host, _countof(host)) != 0)
		return;

	ADDRINFOW hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	ADDRINFOW* result = nullptr;
	if (GetAddrInfoW(host, nullptr, &hints, &result) != 0)
		return;

	for (ADDRINFOW* item = result; item != nullptr; item = item->ai_next)
	{
		WCHAR ip[INET_ADDRSTRLEN] = {};
		if (InetNtopW(AF_INET, &((sockaddr_in*)item->ai_addr)->sin_addr, ip, _countof(ip)) == nullptr)
			continue;
		WCHAR text[128];
		wsprintfW(text, L"Địa chỉ máy chủ: %s:%u", ip, port);
		AddMessage(L"", text);
	}
	FreeAddrInfoW(result);
}

void CChatAppServerDlg::OnBnClickedButtonListen()
{
	if (m_listen != INVALID_SOCKET)
	{
		StopListen(L"Đã dừng máy chủ.");
		return;
	}

	BOOL valid = FALSE;
	UINT port = ::GetDlgItemInt(m_hWnd, IDC_EDIT_PORT, &valid, FALSE);
	if (!valid || port == 0 || port > 65535)
	{
		::MessageBoxW(m_hWnd, L"Nhập cổng từ 1 đến 65535.", L"Lỗi", MB_OK | MB_ICONWARNING);
		return;
	}

	m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_listen == INVALID_SOCKET)
	{
		AddMessage(L"", L"Không tạo được socket.");
		return;
	}

	sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_port = htons((u_short)port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(m_listen, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR)
	{
		if (WSAGetLastError() == WSAEADDRINUSE)
			StopListen(L"Cổng đã được sử dụng. Chọn cổng khác.");
		else
			StopListen(L"Không gắn được socket vào cổng.");
		return;
	}

	if (listen(m_listen, SOMAXCONN) == SOCKET_ERROR)
	{
		StopListen(L"Không lắng nghe được.");
		return;
	}

	if (WSAAsyncSelect(m_listen, m_hWnd, WM_SOCKET, FD_ACCEPT) == SOCKET_ERROR)
	{
		StopListen(L"Không đăng ký được sự kiện socket.");
		return;
	}

	WCHAR text[96];
	wsprintfW(text, L"Đang lắng nghe trên cổng %u...", port);
	AddMessage(L"", text);
	ShowAddresses(port);
	::SetDlgItemTextW(m_hWnd, IDC_BUTTON_LISTEN, L"Dừng");
	EnableItem(IDC_EDIT_PORT, FALSE);
}

void CChatAppServerDlg::StopListen(LPCWSTR text)
{
	if (m_listen != INVALID_SOCKET)
	{
		WSAAsyncSelect(m_listen, m_hWnd, 0, 0);
		closesocket(m_listen);
		m_listen = INVALID_SOCKET;
	}

	CloseClient(nullptr);

	if (m_hWnd == nullptr)
		return;
	EnableItem(IDC_EDIT_PORT, TRUE);
	::SetDlgItemTextW(m_hWnd, IDC_BUTTON_LISTEN, L"Lắng nghe");
	if (text != nullptr && text[0] != L'\0')
		AddMessage(L"", text);
}

void CChatAppServerDlg::CloseClient(LPCWSTR text)
{
	if (m_pending.size > 0)
		AddMessage(L"", L"Tin đang gửi chưa hoàn tất.");

	if (m_client != INVALID_SOCKET)
	{
		WSAAsyncSelect(m_client, m_hWnd, 0, 0);
		closesocket(m_client);
		m_client = INVALID_SOCKET;
	}

	MSG message = {};
	while (::PeekMessageW(&message, m_hWnd, WM_SOCKET, WM_SOCKET, PM_REMOVE))
	{
	}

	BufferFree(m_received);
	BufferFree(m_pending);
	m_sent = 0;
	if (m_message != nullptr)
	{
		HeapFree(GetProcessHeap(), 0, m_message);
		m_message = nullptr;
	}

	if (m_hWnd == nullptr)
		return;
	EnableItem(IDC_BUTTON_SEND, FALSE);
	EnableItem(IDC_EDIT_MESSAGE, TRUE);
	if (text != nullptr && text[0] != L'\0')
		AddMessage(L"", text);

	if (m_listen != INVALID_SOCKET)
		Accept();
}

void CChatAppServerDlg::Accept()
{
	while (m_listen != INVALID_SOCKET)
	{
		sockaddr_in address = {};
		int size = sizeof(address);
		SOCKET accepted = accept(m_listen, (sockaddr*)&address, &size);
		if (accepted == INVALID_SOCKET)
			return;

		WCHAR ip[INET_ADDRSTRLEN] = {};
		InetNtopW(AF_INET, &address.sin_addr, ip, _countof(ip));
		WCHAR text[160];

		if (m_client != INVALID_SOCKET)
		{
			closesocket(accepted);
			wsprintfW(text, L"Từ chối %s:%u vì đang có máy khách khác.", ip, ntohs(address.sin_port));
			AddMessage(L"", text);
			continue;
		}

		m_client = accepted;
		if (WSAAsyncSelect(m_client, m_hWnd, WM_SOCKET, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
		{
			CloseClient(L"Không đăng ký được sự kiện socket cho máy khách.");
			return;
		}

		wsprintfW(text, L"Máy khách %s:%u đã kết nối.", ip, ntohs(address.sin_port));
		AddMessage(L"", text);
		EnableItem(IDC_BUTTON_SEND, TRUE);
		::SetFocus(::GetDlgItem(m_hWnd, IDC_EDIT_MESSAGE));
	}
}

LRESULT CChatAppServerDlg::OnSocket(WPARAM wParam, LPARAM lParam)
{
	SOCKET source = (SOCKET)wParam;
	int event = WSAGETSELECTEVENT(lParam);
	int error = WSAGETSELECTERROR(lParam);

	if (source != INVALID_SOCKET && source == m_listen)
	{
		if (error != 0)
		{
			WCHAR text[96];
			wsprintfW(text, L"Lỗi khi chấp nhận kết nối (mã lỗi %d).", error);
			AddMessage(L"", text);
			return 0;
		}
		if (event == FD_ACCEPT)
			Accept();
		return 0;
	}

	if (source == INVALID_SOCKET || source != m_client)
		return 0;

	if (error != 0)
	{
		WCHAR text[96];
		wsprintfW(text, L"Mất kết nối với máy khách (mã lỗi %d).", error);
		CloseClient(text);
		return 0;
	}

	switch (event)
	{
	case FD_READ:
	case FD_CLOSE:
		Receive();
		break;
	case FD_WRITE:
		Flush();
		break;
	}
	return 0;
}

void CChatAppServerDlg::Receive()
{
	char buffer[8192];
	SOCKET client = m_client;
	while (m_client != INVALID_SOCKET && m_client == client)
	{
		int count = recv(m_client, buffer, sizeof(buffer), 0);
		if (count > 0)
		{
			if (!BufferAppend(m_received, buffer, (SIZE_T)count))
			{
				CloseClient(L"Không đủ bộ nhớ để nhận tin nhắn.");
				return;
			}
			if (!ParseMessages())
				return;
		}
		else if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			return;
		else
		{
			if (m_received.size > 0)
				CloseClient(L"Mất kết nối khi tin nhắn chưa nhận đủ.");
			else
				CloseClient(L"Máy khách đã ngắt kết nối.");
			return;
		}
	}
}

bool CChatAppServerDlg::ParseMessages()
{
	SIZE_T offset = 0;
	while (m_received.size - offset >= 4)
	{
		u_long header = 0;
		CopyMemory(&header, m_received.data + offset, 4);
		u_long length = ntohl(header);
		if (length == 0 || length > MAX_MESSAGE_BYTES || length % sizeof(WCHAR) != 0)
		{
			CloseClient(L"Độ dài tin nhắn không hợp lệ.");
			return false;
		}

		if (m_received.size - offset - 4 < length)
		{
			if (!BufferReserve(m_received, offset + 4 + length))
			{
				CloseClient(L"Không đủ bộ nhớ để nhận tin nhắn.");
				return false;
			}
			break;
		}

		SIZE_T count = length / sizeof(WCHAR);
		WCHAR* text = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (count + 1) * sizeof(WCHAR));
		if (text == nullptr)
		{
			CloseClient(L"Không đủ bộ nhớ để hiển thị tin nhắn.");
			return false;
		}
		CopyMemory(text, m_received.data + offset + 4, length);
		text[count] = L'\0';
		AddMessage(L"Máy khách: ", text);
		HeapFree(GetProcessHeap(), 0, text);
		offset += 4 + (SIZE_T)length;
	}
	BufferConsume(m_received, offset);
	return true;
}

void CChatAppServerDlg::OnBnClickedButtonSend()
{
	if (m_client == INVALID_SOCKET || m_pending.size > 0)
		return;

	HWND edit = ::GetDlgItem(m_hWnd, IDC_EDIT_MESSAGE);
	int length = ::GetWindowTextLengthW(edit);
	if (length <= 0)
		return;
	if ((DWORD)length > MAX_MESSAGE_BYTES / sizeof(WCHAR))
	{
		::MessageBoxW(m_hWnd, L"Tin nhắn quá dài.", L"Lỗi", MB_OK | MB_ICONWARNING);
		return;
	}

	m_message = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, ((SIZE_T)length + 1) * sizeof(WCHAR));
	if (m_message == nullptr)
	{
		AddMessage(L"", L"Không đủ bộ nhớ để gửi tin nhắn.");
		return;
	}

	length = ::GetWindowTextW(edit, m_message, length + 1);
	if (length <= 0)
	{
		HeapFree(GetProcessHeap(), 0, m_message);
		m_message = nullptr;
		return;
	}

	u_long bytes = (u_long)length * sizeof(WCHAR);
	u_long header = htonl(bytes);
	if (!BufferReserve(m_pending, 4 + (SIZE_T)bytes)
		|| !BufferAppend(m_pending, &header, 4)
		|| !BufferAppend(m_pending, m_message, bytes))
	{
		BufferFree(m_pending);
		HeapFree(GetProcessHeap(), 0, m_message);
		m_message = nullptr;
		AddMessage(L"", L"Không đủ bộ nhớ để gửi tin nhắn.");
		return;
	}
	m_sent = 0;

	EnableItem(IDC_BUTTON_SEND, FALSE);
	EnableItem(IDC_EDIT_MESSAGE, FALSE);
	Flush();
}

void CChatAppServerDlg::Flush()
{
	if (m_pending.size == 0)
		return;

	while (m_sent < m_pending.size)
	{
		SIZE_T remaining = m_pending.size - m_sent;
		int length = IO_CHUNK;
		if (remaining < (SIZE_T)IO_CHUNK)
			length = (int)remaining;
		int count = send(m_client, (const char*)m_pending.data + m_sent, length, 0);
		if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			return;
		if (count <= 0)
		{
			CloseClient(L"Gửi thất bại. Máy khách đã ngắt kết nối.");
			return;
		}
		m_sent += (SIZE_T)count;
	}

	AddMessage(L"Tôi: ", m_message);
	BufferFree(m_pending);
	HeapFree(GetProcessHeap(), 0, m_message);
	m_message = nullptr;
	m_sent = 0;

	::SetDlgItemTextW(m_hWnd, IDC_EDIT_MESSAGE, L"");
	EnableItem(IDC_EDIT_MESSAGE, TRUE);
	EnableItem(IDC_BUTTON_SEND, TRUE);
	::SetFocus(::GetDlgItem(m_hWnd, IDC_EDIT_MESSAGE));
}

void CChatAppServerDlg::OnBnClickedCheckAutorun()
{
	bool enable = ::IsDlgButtonChecked(m_hWnd, IDC_CHECK_AUTORUN) == BST_CHECKED;
	LSTATUS status = SetAutorun(enable);
	if (status != ERROR_SUCCESS)
	{
		// Ghi registry lỗi: trả checkbox về trạng thái cũ
		::CheckDlgButton(m_hWnd, IDC_CHECK_AUTORUN, enable ? BST_UNCHECKED : BST_CHECKED);
		WCHAR text[128];
		wsprintfW(text, L"Không ghi được registry (mã lỗi %ld).", status);
		::MessageBoxW(m_hWnd, text, L"Lỗi", MB_OK | MB_ICONWARNING);
		return;
	}
	AddMessage(L"", enable ? L"Đã bật khởi động cùng Windows." : L"Đã tắt khởi động cùng Windows.");
}

BOOL CChatAppServerDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN && ::GetKeyState(VK_CONTROL) < 0)
	{
		OnBnClickedButtonSend();
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CChatAppServerDlg::OnDestroy()
{
	StopListen(nullptr);
	if (m_started)
		WSACleanup();
	CDialogEx::OnDestroy();
}

void CChatAppServerDlg::OnOK()
{
}
