
// ChatAppDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "ChatApp.h"
#include "ChatAppDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\Task03_MFC_Chat_Pipe";
static const wchar_t* CLIENT_TITLE = L"ChatClient";
#define WM_PIPE_MESSAGE (WM_APP + 1)

namespace
{
	const DWORD PIPE_WRITE_TIMEOUT_MS = 2000;

	BOOL ReadPipe(HANDLE pipe, void* buffer, DWORD bytesToRead, DWORD* bytesRead)
	{
		OVERLAPPED overlapped = {};
		overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (overlapped.hEvent == NULL)
			return FALSE;

		BOOL result = ReadFile(pipe, buffer, bytesToRead, bytesRead, &overlapped);
		if (!result && GetLastError() == ERROR_IO_PENDING)
		{
			result = (WaitForSingleObject(overlapped.hEvent, INFINITE) == WAIT_OBJECT_0) &&
				GetOverlappedResult(pipe, &overlapped, bytesRead, FALSE);
		}

		CloseHandle(overlapped.hEvent);
		return result;
	}

	BOOL WritePipeWithTimeout(HANDLE pipe, const void* buffer, DWORD bytesToWrite, DWORD* bytesWritten)
	{
		OVERLAPPED overlapped = {};
		overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (overlapped.hEvent == NULL)
			return FALSE;

		BOOL result = WriteFile(pipe, buffer, bytesToWrite, bytesWritten, &overlapped);
		if (!result && GetLastError() == ERROR_IO_PENDING)
		{
			DWORD waitResult = WaitForSingleObject(overlapped.hEvent, PIPE_WRITE_TIMEOUT_MS);
			if (waitResult == WAIT_OBJECT_0)
			{
				result = GetOverlappedResult(pipe, &overlapped, bytesWritten, FALSE);
			}
			else
			{
				CancelIoEx(pipe, &overlapped);
				WaitForSingleObject(overlapped.hEvent, INFINITE);
				SetLastError(waitResult == WAIT_TIMEOUT ? ERROR_TIMEOUT : ERROR_OPERATION_ABORTED);
				result = FALSE;
			}
		}

		CloseHandle(overlapped.hEvent);
		return result;
	}
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


// CChatAppDlg dialog



CChatAppDlg::CChatAppDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CHATAPP_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_hPipe = INVALID_HANDLE_VALUE;
	m_bPipeConnected = FALSE;
	m_bStopping = FALSE;
	m_hPipeThread = NULL;
}

void CChatAppDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CChatAppDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_WINDOWS, &CChatAppDlg::OnBnClickedButtonWindows)
	ON_BN_CLICKED(IDC_BUTTON_PIPE, &CChatAppDlg::OnBnClickedButtonPipe)
	ON_WM_COPYDATA()
	ON_MESSAGE(WM_PIPE_MESSAGE, &CChatAppDlg::OnPipeMessage)
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CChatAppDlg message handlers

BOOL CChatAppDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
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

	// TODO: Add extra initialization here
	m_hPipeThread = CreateThread(NULL, 0, PipeThreadProc, this, 0, NULL);

	if (m_hPipeThread == NULL)
	{
		MessageBoxW(L"Không tạo được Pipe Thread!", L"Lỗi", MB_OK | MB_ICONERROR);
	}
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CChatAppDlg::OnDestroy()
{
	HANDLE pipe = INVALID_HANDLE_VALUE;
	{
		CSingleLock lock(&m_pipeLock, TRUE);
		m_bStopping = TRUE;
		pipe = m_hPipe;
		if (pipe != INVALID_HANDLE_VALUE)
			CancelIoEx(pipe, NULL);
	}

	if (m_hPipeThread != NULL)
	{
		WaitForSingleObject(m_hPipeThread, INFINITE);
		CloseHandle(m_hPipeThread);
		m_hPipeThread = NULL;
	}

	CDialogEx::OnDestroy();
}

void CChatAppDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CChatAppDlg::OnPaint()
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
HCURSOR CChatAppDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

LRESULT CChatAppDlg::OnPipeMessage(WPARAM wParam, LPARAM lParam)
{
	CString* text = (CString*)lParam;
	if (text != NULL)
	{
		AddMessage(*text);
		delete text;
	}
	return 0;
}

void CChatAppDlg::AddMessage(CString text)
{
	CEdit* editHistory = (CEdit*)GetDlgItem(IDC_EDIT_HISTORY);
	int len = editHistory->GetWindowTextLength();
	editHistory->SetSel(len, len);
	editHistory->ReplaceSel(text + L"\r\n");
}

BOOL CChatAppDlg::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct)
{
	if (pCopyDataStruct == NULL)
		return FALSE;

	if (pCopyDataStruct->lpData == NULL)
		return FALSE;

	CString message =
		(LPCWSTR)pCopyDataStruct->lpData;

	CString display;

	display = L"[Windows Message] Client: ";
	display += message;

	AddMessage(display);

	return TRUE;
}

void CChatAppDlg::OnBnClickedButtonWindows()
{
	CString message;

	GetDlgItemText(IDC_EDIT_MESSAGE,message);

	if (message.IsEmpty())
		return;

	HWND hClient = ::FindWindow(NULL, CLIENT_TITLE);

	if (hClient == NULL)
	{
		MessageBoxW(L"Không tìm thấy Client!", L"Lỗi", MB_OK | MB_ICONERROR);
		return;
	}
	
	COPYDATASTRUCT data;
	data.dwData = 1;
	data.cbData = (message.GetLength() + 1) * sizeof(wchar_t);
	data.lpData = (PVOID)(LPCWSTR)message;
	
	::SendMessage(hClient, WM_COPYDATA, (WPARAM)m_hWnd, (LPARAM)&data);
	
	CString display;
	display = L"[Windows Message] Server: ";
	display += message;
	AddMessage(display);
	SetDlgItemText(IDC_EDIT_MESSAGE, L"");
}


DWORD WINAPI CChatAppDlg::PipeThreadProc(LPVOID pParam)
{
	CChatAppDlg* dlg = (CChatAppDlg*)pParam;
	dlg->PipeThread();
	return 0;
}

void CChatAppDlg::PipeThread()
{
	while (!m_bStopping)
	{
		HANDLE pipe = CreateNamedPipe(PIPE_NAME,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			1, 4096, 4096, 0, NULL);
		if (pipe == INVALID_HANDLE_VALUE)
			return;

		{
			CSingleLock lock(&m_pipeLock, TRUE);
			if (m_bStopping)
			{
				CloseHandle(pipe);
				return;
			}
			m_hPipe = pipe;
		}

		OVERLAPPED connectOverlapped = {};
		connectOverlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		BOOL connected = FALSE;
		if (connectOverlapped.hEvent != NULL)
		{
			connected = ConnectNamedPipe(pipe, &connectOverlapped);
			if (!connected)
			{
				DWORD error = GetLastError();
				if (error == ERROR_PIPE_CONNECTED)
					connected = TRUE;
				else if (error == ERROR_IO_PENDING &&
					WaitForSingleObject(connectOverlapped.hEvent, INFINITE) == WAIT_OBJECT_0)
				{
					DWORD ignored = 0;
					connected = GetOverlappedResult(pipe, &connectOverlapped, &ignored, FALSE);
				}
			}
			CloseHandle(connectOverlapped.hEvent);
		}

		if (connected)
		{
			CSingleLock lock(&m_pipeLock, TRUE);
			m_bPipeConnected = TRUE;
		}

		while (connected && !m_bStopping)
		{
			wchar_t buffer[1024] = { 0 };
			DWORD bytesRead = 0;
			BOOL result = ReadPipe(pipe, buffer, sizeof(buffer) - sizeof(wchar_t), &bytesRead);
			if (!result || bytesRead == 0)
				break;

			DWORD charCount = bytesRead / sizeof(wchar_t);
			buffer[min(charCount, static_cast<DWORD>(_countof(buffer) - 1))] = L'\0';
			CString* text = new CString(CString(L"[Named Pipe] Client: ") + buffer);
			if (!PostMessage(WM_PIPE_MESSAGE, 0, reinterpret_cast<LPARAM>(text)))
				delete text;
		}

		{
			CSingleLock lock(&m_pipeLock, TRUE);
			m_bPipeConnected = FALSE;
			if (m_hPipe == pipe)
				m_hPipe = INVALID_HANDLE_VALUE;
		}
		if (connected)
			DisconnectNamedPipe(pipe);
		CloseHandle(pipe);
	}
}

BOOL CChatAppDlg::WritePipeMessage(const CString& message)
{
	CSingleLock lock(&m_pipeLock, TRUE);
	if (m_hPipe == INVALID_HANDLE_VALUE || !m_bPipeConnected)
	{
		SetLastError(ERROR_PIPE_NOT_CONNECTED);
		return FALSE;
	}

	DWORD bytesWritten = 0;
	return WritePipeWithTimeout(m_hPipe, static_cast<LPCWSTR>(message),
		(message.GetLength() + 1) * sizeof(wchar_t), &bytesWritten);
}

void CChatAppDlg::OnBnClickedButtonPipe()
{
	CString message;
	GetDlgItemText(IDC_EDIT_MESSAGE, message);

	if (message.IsEmpty())
		return;

	if (!WritePipeMessage(message))
	{
		MessageBoxW(GetLastError() == ERROR_TIMEOUT
			? L"Named Pipe không phản hồi (quá 2 giây)."
			: L"Client chưa kết nối hoặc gửi bằng Named Pipe thất bại!",
			L"Lỗi", MB_OK | MB_ICONERROR);
		return;
	}

	CString display;
	display = L"[Named Pipe] Server: ";
	display += message;
	AddMessage(display);
	SetDlgItemText(IDC_EDIT_MESSAGE, L"");
}
