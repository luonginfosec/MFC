#include "pch.h"
#include "framework.h"
#include "ChatAppClient.h"
#include "ChatAppClientDlg.h"
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

// Giải phóng bộ đệm
static void BufferFree(ByteBuffer& buffer)
{
	if (buffer.data != nullptr)
		HeapFree(GetProcessHeap(), 0, buffer.data);
	buffer.data = nullptr;
	buffer.size = 0;
	buffer.capacity = 0;
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


// CChatAppClientDlg dialog



CChatAppClientDlg::CChatAppClientDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CHATAPPCLIENT_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CChatAppClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CChatAppClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CChatAppClientDlg::OnBnClickedButtonConnect)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CChatAppClientDlg::OnBnClickedButtonSend)
	ON_MESSAGE(WM_SOCKET, &CChatAppClientDlg::OnSocket)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CChatAppClientDlg message handlers

BOOL CChatAppClientDlg::OnInitDialog()
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

	::SetWindowTextW(m_hWnd, L"Chat TCP - Máy khách");

	::SetDlgItemTextW(m_hWnd, IDC_EDIT_IP, L"127.0.0.1");
	::SetDlgItemTextW(m_hWnd, IDC_EDIT_PORT, L"5000");

	::SendMessageW(::GetDlgItem(m_hWnd, IDC_EDIT_MESSAGE), EM_SETLIMITTEXT, 0, 0);
	::SendMessageW(::GetDlgItem(m_hWnd, IDC_EDIT_HISTORY), EM_SETLIMITTEXT, 0, 0);

	EnableItem(IDC_BUTTON_SEND, FALSE);

	WSADATA data = {};
	m_started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
	if (!m_started)
	{
		AddMessage(L"", L"Không khởi tạo được Winsock.");
		EnableItem(IDC_BUTTON_CONNECT, FALSE);
	}

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CChatAppClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CChatAppClientDlg::OnPaint()
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
HCURSOR CChatAppClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CChatAppClientDlg::EnableItem(int id, BOOL enable)
{
	::EnableWindow(::GetDlgItem(m_hWnd, id), enable);
}

void CChatAppClientDlg::AddMessage(LPCWSTR prefix, LPCWSTR text)
{
	HWND history = ::GetDlgItem(m_hWnd, IDC_EDIT_HISTORY);

	int length = ::GetWindowTextLengthW(history);

	::SendMessageW(history, EM_SETSEL, (WPARAM)length, (LPARAM)length);
	::SendMessageW(history, EM_REPLACESEL, FALSE, (LPARAM)prefix);
	::SendMessageW(history, EM_REPLACESEL, FALSE, (LPARAM)text);
	::SendMessageW(history, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
}

void CChatAppClientDlg::Stop(LPCWSTR text)
{
	if (m_pending.size > 0)
		AddMessage(L"", L"Tin đang gửi chưa hoàn tất.");

	if (m_socket != INVALID_SOCKET)
	{
		WSAAsyncSelect(m_socket, m_hWnd, 0, 0);
		closesocket(m_socket);
		m_socket = INVALID_SOCKET;
	}

	MSG message = {};
	while (::PeekMessageW(&message, m_hWnd, WM_SOCKET, WM_SOCKET, PM_REMOVE))
	{
	}

	m_connected = false;
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
	EnableItem(IDC_EDIT_IP, TRUE);
	EnableItem(IDC_EDIT_PORT, TRUE);
	::SetDlgItemTextW(m_hWnd, IDC_BUTTON_CONNECT, L"Kết nối");
	if (text != nullptr && text[0] != L'\0')
		AddMessage(L"", text);
}

void CChatAppClientDlg::OnBnClickedButtonConnect()
{
	if (m_socket != INVALID_SOCKET)
	{
		Stop(L"Đã ngắt kết nối.");
		return;
	}

	BOOL valid = FALSE;
	UINT port = ::GetDlgItemInt(m_hWnd, IDC_EDIT_PORT, &valid, FALSE);
	if (!valid || port == 0 || port > 65535)
	{
		::MessageBoxW(m_hWnd, L"Nhập cổng từ 1 đến 65535.", L"Lỗi", MB_OK | MB_ICONWARNING);
		return;
	}

	WCHAR ip[64] = {};
	::GetDlgItemTextW(m_hWnd, IDC_EDIT_IP, ip, _countof(ip));

	sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_port = htons((u_short)port);
	if (InetPtonW(AF_INET, ip, &address.sin_addr) != 1)
	{
		::MessageBoxW(m_hWnd, L"Nhập địa chỉ IPv4 của máy chủ (vd: 192.168.1.10).", L"Lỗi", MB_OK | MB_ICONWARNING);
		return;
	}

	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == INVALID_SOCKET)
	{
		AddMessage(L"", L"Không tạo được socket.");
		return;
	}

	if (WSAAsyncSelect(m_socket, m_hWnd, WM_SOCKET, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
	{
		Stop(L"Không đăng ký được sự kiện socket.");
		return;
	}

	if (connect(m_socket, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR
		&& WSAGetLastError() != WSAEWOULDBLOCK)
	{
		Stop(L"Không kết nối được. Kiểm tra IP, cổng và tường lửa.");
		return;
	}

	AddMessage(L"", L"Đang kết nối...");
	::SetDlgItemTextW(m_hWnd, IDC_BUTTON_CONNECT, L"Ngắt");
	EnableItem(IDC_EDIT_IP, FALSE);
	EnableItem(IDC_EDIT_PORT, FALSE);
}

void CChatAppClientDlg::Connected()
{
	m_connected = true;
	EnableItem(IDC_BUTTON_SEND, TRUE);
	AddMessage(L"", L"Đã kết nối tới máy chủ.");
	// https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setfocus
	::SetFocus(::GetDlgItem(m_hWnd, IDC_EDIT_MESSAGE));
}

LRESULT CChatAppClientDlg::OnSocket(WPARAM wParam, LPARAM lParam)
{
	if ((SOCKET)wParam != m_socket || m_socket == INVALID_SOCKET)
		return 0;
	int event = WSAGETSELECTEVENT(lParam);
	int error = WSAGETSELECTERROR(lParam);

	if (error != 0)
	{
		WCHAR text[160];
		if (event == FD_CONNECT)
			wsprintfW(text, L"Không kết nối được tới máy chủ (mã lỗi %d). Kiểm tra IP, cổng và tường lửa.", error);
		else
			wsprintfW(text, L"Mất kết nối (mã lỗi %d).", error);
		Stop(text);
		return 0;
	}

	switch (event)
	{
	case FD_CONNECT:
		Connected();
		break;
	case FD_READ:
	case FD_CLOSE:		
		Receive();
		break;
	case FD_WRITE:
		if (m_connected)
			Flush();
		break;
	}
	return 0;
}

void CChatAppClientDlg::Receive()
{
	char buffer[8192];
	while (m_socket != INVALID_SOCKET)
	{
		// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-recv
		int count = recv(m_socket, buffer, sizeof(buffer), 0);
		if (count > 0)
		{
			if (!BufferAppend(m_received, buffer, (SIZE_T)count))
			{
				Stop(L"Không đủ bộ nhớ để nhận tin nhắn.");
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
				Stop(L"Mất kết nối khi tin nhắn chưa nhận đủ.");
			else
				Stop(L"Máy chủ đã đóng kết nối. Bấm \"Kết nối\" để kết nối lại.");
			return;
		}
	}
}

bool CChatAppClientDlg::ParseMessages()
{
	SIZE_T offset = 0;
	while (m_received.size - offset >= 4)
	{
		u_long header = 0;
		CopyMemory(&header, m_received.data + offset, 4);
		u_long length = ntohl(header);
		if (length == 0 || length > MAX_MESSAGE_BYTES || length % sizeof(WCHAR) != 0)
		{
			Stop(L"Độ dài tin nhắn không hợp lệ.");
			return false;
		}
		
		if (m_received.size - offset - 4 < length)
		{
			if (!BufferReserve(m_received, offset + 4 + length))
			{
				Stop(L"Không đủ bộ nhớ để nhận tin nhắn.");
				return false;
			}
			break;
		}

		SIZE_T count = length / sizeof(WCHAR);
		WCHAR* text = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, (count + 1) * sizeof(WCHAR));
		if (text == nullptr)
		{
			Stop(L"Không đủ bộ nhớ để hiển thị tin nhắn.");
			return false;
		}
		CopyMemory(text, m_received.data + offset + 4, length);
		text[count] = L'\0';
		AddMessage(L"Máy chủ: ", text);
		HeapFree(GetProcessHeap(), 0, text);
		offset += 4 + (SIZE_T)length;
	}
	BufferConsume(m_received, offset);
	return true;
}

void CChatAppClientDlg::OnBnClickedButtonSend()
{
	if (!m_connected || m_pending.size > 0)
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
	// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-htonl
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

void CChatAppClientDlg::Flush()
{
	if (m_pending.size == 0)
		return;

	while (m_sent < m_pending.size)
	{
		SIZE_T remaining = m_pending.size - m_sent;
		int length = IO_CHUNK;
		if (remaining < (SIZE_T)IO_CHUNK)
			length = (int)remaining;
		// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-send
		int count = send(m_socket, (const char*)m_pending.data + m_sent, length, 0);
		if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
			return;
		if (count <= 0)
		{
			Stop(L"Gửi thất bại. Kết nối đã đóng.");
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

BOOL CChatAppClientDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN && ::GetKeyState(VK_CONTROL) < 0)
	{
		OnBnClickedButtonSend();
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}


void CChatAppClientDlg::OnDestroy()
{
	Stop(nullptr);
	if (m_started)
		WSACleanup();
	CDialogEx::OnDestroy();
}

void CChatAppClientDlg::OnOK()
{
}
