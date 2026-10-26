#include "pch.h"
#include "framework.h"
#include "ChatAppClient.h"
#include "ChatAppClientDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

const UINT WM_CHAT_RECV = WM_APP + 1;
const UINT WM_CHAT_CLOSED = WM_APP + 2;
const u_long MAX_MSG_BYTES = 10 * 1024 * 1024;
const WCHAR RUN_KEY[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const WCHAR RUN_VALUE[] = L"ChatAppClient";

struct RecvParam
{
	HWND hWnd;
	SOCKET sock;
};

static bool SendAll(SOCKET s, const char* buf, int len)
{
	while (len > 0)
	{
		int n = send(s, buf, len, 0);
		if (n <= 0)
			return false;
		buf += n;
		len -= n;
	}
	return true;
}

static bool RecvAll(SOCKET s, char* buf, int len)
{
	while (len > 0)
	{
		int n = recv(s, buf, len, 0);
		if (n <= 0)
			return false;
		buf += n;
		len -= n;
	}
	return true;
}

static bool SendMsg(SOCKET s, const CString& text)
{
	u_long bytes = text.GetLength() * sizeof(WCHAR);
	u_long header = htonl(bytes);
	return SendAll(s, (const char*)&header, 4) && SendAll(s, (const char*)(LPCWSTR)text, bytes);
}

static bool RecvMsg(SOCKET s, CString& text)
{
	u_long header = 0;
	if (!RecvAll(s, (char*)&header, 4))
		return false;
	u_long bytes = ntohl(header);
	if (bytes == 0 || bytes > MAX_MSG_BYTES || bytes % sizeof(WCHAR) != 0)
		return false;

	int count = bytes / sizeof(WCHAR);
	bool ok = RecvAll(s, (char*)text.GetBuffer(count), bytes);
	text.ReleaseBuffer(ok ? count : 0);
	return ok;
}

static bool IsAutorunEnabled()
{
	return RegGetValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
}

static bool SetAutorun(bool enable)
{
	if (!enable)
		return RegDeleteKeyValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE) == ERROR_SUCCESS;

	WCHAR path[MAX_PATH];
	if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0)
		return false;
	CString command = L"\"" + CString(path) + L"\"";
	return RegSetKeyValueW(HKEY_CURRENT_USER, RUN_KEY, RUN_VALUE, REG_SZ,
		(LPCWSTR)command, (command.GetLength() + 1) * sizeof(WCHAR)) == ERROR_SUCCESS;
}

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

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

CChatAppClientDlg::CChatAppClientDlg(CWnd* pParent)
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
	ON_WM_DESTROY()
	ON_MESSAGE(WM_CHAT_RECV, &CChatAppClientDlg::OnChatRecv)
	ON_MESSAGE(WM_CHAT_CLOSED, &CChatAppClientDlg::OnChatClosed)
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CChatAppClientDlg::OnBnClickedButtonConnect)
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CChatAppClientDlg::OnBnClickedButtonSend)
	ON_BN_CLICKED(IDC_CHECK_AUTORUN, &CChatAppClientDlg::OnBnClickedCheckAutorun)
END_MESSAGE_MAP()

BOOL CChatAppClientDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		AfxMessageBox(L"Không khởi tạo được Winsock.");
		EndDialog(IDCANCEL);
		return TRUE;
	}

	SetWindowText(L"Chat TCP - Máy khách");
	SetDlgItemText(IDC_EDIT_IP, L"127.0.0.1");
	SetDlgItemText(IDC_EDIT_PORT, L"9999");
	((CEdit*)GetDlgItem(IDC_EDIT_MESSAGE))->SetLimitText(0);
	((CEdit*)GetDlgItem(IDC_EDIT_HISTORY))->SetLimitText(0);
	CheckDlgButton(IDC_CHECK_AUTORUN, IsAutorunEnabled() ? BST_CHECKED : BST_UNCHECKED);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	return TRUE;
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

void CChatAppClientDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CChatAppClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CChatAppClientDlg::AddMessage(const CString& text)
{
	CEdit* history = (CEdit*)GetDlgItem(IDC_EDIT_HISTORY);
	int length = history->GetWindowTextLength();
	history->SetSel(length, length);
	history->ReplaceSel(text + L"\r\n");
}

void CChatAppClientDlg::Disconnect(const CString& text)
{
	if (m_sock == INVALID_SOCKET)
		return;
	closesocket(m_sock);
	m_sock = INVALID_SOCKET;

	SetDlgItemText(IDC_BUTTON_CONNECT, L"Kết nối");
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT_IP)->EnableWindow(TRUE);
	GetDlgItem(IDC_EDIT_PORT)->EnableWindow(TRUE);
	AddMessage(text);
}

UINT CChatAppClientDlg::RecvThread(LPVOID param)
{
	RecvParam* p = (RecvParam*)param;
	CString text;
	while (RecvMsg(p->sock, text))
	{
		CString* msg = new CString(text);
		if (!::PostMessage(p->hWnd, WM_CHAT_RECV, (WPARAM)p->sock, (LPARAM)msg))
			delete msg;
	}
	::PostMessage(p->hWnd, WM_CHAT_CLOSED, (WPARAM)p->sock, 0);
	delete p;
	return 0;
}

LRESULT CChatAppClientDlg::OnChatRecv(WPARAM wParam, LPARAM lParam)
{
	CString* msg = (CString*)lParam;
	if ((SOCKET)wParam == m_sock)
		AddMessage(L"Echo: " + *msg);
	delete msg;
	return 0;
}

LRESULT CChatAppClientDlg::OnChatClosed(WPARAM wParam, LPARAM)
{
	if ((SOCKET)wParam == m_sock)
		Disconnect(L"Máy chủ đã đóng kết nối.");
	return 0;
}

void CChatAppClientDlg::OnBnClickedButtonConnect()
{
	if (m_sock != INVALID_SOCKET)
	{
		Disconnect(L"Đã ngắt kết nối.");
		return;
	}

	CString ip;
	GetDlgItemText(IDC_EDIT_IP, ip);
	UINT port = GetDlgItemInt(IDC_EDIT_PORT, nullptr, FALSE);
	if (port == 0 || port > 65535)
	{
		AfxMessageBox(L"Nhập cổng từ 1 đến 65535.");
		return;
	}

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short)port);
	if (InetPtonW(AF_INET, ip, &addr.sin_addr) != 1)
	{
		AfxMessageBox(L"Địa chỉ IP không hợp lệ.");
		return;
	}

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connect(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		CString e;
		e.Format(L"Không kết nối được (mã lỗi %d).", WSAGetLastError());
		AfxMessageBox(e);
		closesocket(s);
		return;
	}
	m_sock = s;

	SetDlgItemText(IDC_BUTTON_CONNECT, L"Ngắt");
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(TRUE);
	GetDlgItem(IDC_EDIT_IP)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT_PORT)->EnableWindow(FALSE);
	GetDlgItem(IDC_EDIT_MESSAGE)->SetFocus();
	AddMessage(L"Đã kết nối tới máy chủ.");

	AfxBeginThread(RecvThread, new RecvParam{ m_hWnd, s });
}

void CChatAppClientDlg::OnBnClickedButtonSend()
{
	if (m_sock == INVALID_SOCKET)
		return;

	CString text;
	GetDlgItemText(IDC_EDIT_MESSAGE, text);
	if (text.IsEmpty())
		return;

	if (!SendMsg(m_sock, text))
	{
		Disconnect(L"Gửi thất bại. Kết nối đã đóng.");
		return;
	}
	AddMessage(L"Tôi: " + text);
	SetDlgItemText(IDC_EDIT_MESSAGE, L"");
}

void CChatAppClientDlg::OnBnClickedCheckAutorun()
{
	bool enable = IsDlgButtonChecked(IDC_CHECK_AUTORUN) == BST_CHECKED;
	if (!SetAutorun(enable))
	{
		CheckDlgButton(IDC_CHECK_AUTORUN, enable ? BST_UNCHECKED : BST_CHECKED);
		AfxMessageBox(L"Không ghi được registry.");
		return;
	}
	AddMessage(enable ? L"Đã bật khởi động cùng Windows." : L"Đã tắt khởi động cùng Windows.");
}

BOOL CChatAppClientDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN && GetKeyState(VK_CONTROL) < 0)
	{
		OnBnClickedButtonSend();
		return TRUE;
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CChatAppClientDlg::OnDestroy()
{
	if (m_sock != INVALID_SOCKET)
	{
		closesocket(m_sock);
		m_sock = INVALID_SOCKET;
	}
	WSACleanup();
	CDialogEx::OnDestroy();
}

void CChatAppClientDlg::OnOK()
{
}
