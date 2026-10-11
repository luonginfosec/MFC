#include "pch.h"
#include "framework.h"
#include "ChatApp.h"
#include "ChatAppDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\Task03_MFC_Chat_Pipe";
static const wchar_t* SERVER_TITLE = L"ChatServer";

#define WM_PIPE_MESSAGE (WM_APP + 1)
#define WM_PIPE_DISCONNECTED (WM_APP + 2)
#define PIPE_CONNECT_TIMER 1

namespace
{
    const DWORD PIPE_WRITE_TIMEOUT_MS = 2000;

    DWORD ReadPipePart(HANDLE pipe, wchar_t* buffer, DWORD bufferSize, DWORD* bytesRead)
    {
        *bytesRead = 0;

        OVERLAPPED ov = {};
        ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (ov.hEvent == NULL)
            return GetLastError();

        BOOL result = ReadFile(pipe, buffer, bufferSize, bytesRead, &ov);
        DWORD error = ERROR_SUCCESS;

        if (!result)
        {
            error = GetLastError();
            if (error == ERROR_IO_PENDING)
            {
                DWORD wait = WaitForSingleObject(ov.hEvent, INFINITE);
                if (wait == WAIT_OBJECT_0)
                {
                    result = GetOverlappedResult(pipe, &ov, bytesRead, FALSE);
                    if (result)
                        error = ERROR_SUCCESS;
                    else
                        error = GetLastError();
                }
                else
                {
                    error = ERROR_OPERATION_ABORTED;
                }
            }
        }

        if (error == ERROR_MORE_DATA && *bytesRead == 0)
            *bytesRead = bufferSize;

        CloseHandle(ov.hEvent);
        return error;
    }

    BOOL WritePipeWithTimeout(HANDLE pipe, const void* buffer, DWORD bytesToWrite)
    {
        OVERLAPPED ov = {};
        ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (ov.hEvent == NULL)
            return FALSE;

        DWORD bytesWritten = 0;
        BOOL result = WriteFile(pipe, buffer, bytesToWrite, &bytesWritten, &ov);
        DWORD error = ERROR_SUCCESS;

        if (!result)
        {
            error = GetLastError();
            if (error == ERROR_IO_PENDING)
            {
                DWORD wait = WaitForSingleObject(ov.hEvent, PIPE_WRITE_TIMEOUT_MS);
                if (wait == WAIT_OBJECT_0)
                {
                    result = GetOverlappedResult(pipe, &ov, &bytesWritten, FALSE);
                    if (!result)
                        error = GetLastError();
                }
                else
                {
                    CancelIoEx(pipe, &ov);
                    WaitForSingleObject(ov.hEvent, INFINITE);

                    if (wait == WAIT_TIMEOUT)
                        error = ERROR_TIMEOUT;
                    else
                        error = ERROR_OPERATION_ABORTED;
                }
            }
        }

        if (result && bytesWritten != bytesToWrite)
        {
            result = FALSE;
            error = ERROR_WRITE_FAULT;
        }

        CloseHandle(ov.hEvent);
        if (!result)
            SetLastError(error);
        return result;
    }
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

CChatAppDlg::CChatAppDlg(CWnd* pParent) : CDialogEx(IDD_CHATAPP_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_hPipe = INVALID_HANDLE_VALUE;
    m_hPipeThread = NULL;
    m_bStopping = FALSE;
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
    ON_MESSAGE(WM_PIPE_DISCONNECTED, &CChatAppDlg::OnPipeDisconnected)
    ON_WM_TIMER()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CChatAppDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != NULL)
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

    CEdit* editMessage = (CEdit*)GetDlgItem(IDC_EDIT_MESSAGE);
    CEdit* editHistory = (CEdit*)GetDlgItem(IDC_EDIT_HISTORY);
    if (editMessage != NULL)
        editMessage->LimitText(0);
    if (editHistory != NULL)
        editHistory->LimitText(0);

    if (!ConnectPipe())
        SetTimer(PIPE_CONNECT_TIMER, 500, NULL);

    return TRUE;
}

void CChatAppDlg::OnDestroy()
{
    KillTimer(PIPE_CONNECT_TIMER);

    {
        CSingleLock lock(&m_pipeLock, TRUE);
        m_bStopping = TRUE;
        if (m_hPipe != INVALID_HANDLE_VALUE)
            CancelIoEx(m_hPipe, NULL);
    }

    if (m_hPipeThread != NULL)
    {
        WaitForSingleObject(m_hPipeThread, INFINITE);
        CloseHandle(m_hPipeThread);
        m_hPipeThread = NULL;
    }

    CDialogEx::OnDestroy();
}

void CChatAppDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == PIPE_CONNECT_TIMER && ConnectPipe())
        KillTimer(PIPE_CONNECT_TIMER);

    CDialogEx::OnTimer(nIDEvent);
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

void CChatAppDlg::OnPaint()
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

HCURSOR CChatAppDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}

BOOL CChatAppDlg::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct)
{
    if (pCopyDataStruct == NULL || pCopyDataStruct->lpData == NULL)
        return FALSE;
    if (pCopyDataStruct->cbData % sizeof(wchar_t) != 0)
        return FALSE;

    int charCount = (int)(pCopyDataStruct->cbData / sizeof(wchar_t));
    const wchar_t* buffer = (const wchar_t*)pCopyDataStruct->lpData;
    if (charCount > 0 && buffer[charCount - 1] == L'\0')
        charCount--;

    CString message(buffer, charCount);
    AddMessage(L"[Windows Message] Server: " + message);
    return TRUE;
}

LRESULT CChatAppDlg::OnPipeMessage(WPARAM wParam, LPARAM lParam)
{
    CString* text = reinterpret_cast<CString*>(lParam);
    if (text != NULL)
    {
        AddMessage(*text);
        delete text;
    }
    return 0;
}

LRESULT CChatAppDlg::OnPipeDisconnected(WPARAM wParam, LPARAM lParam)
{
    if (m_hPipeThread != NULL)
    {
        WaitForSingleObject(m_hPipeThread, INFINITE);
        CloseHandle(m_hPipeThread);
        m_hPipeThread = NULL;
    }

    if (!m_bStopping)
        SetTimer(PIPE_CONNECT_TIMER, 500, NULL);

    return 0;
}

void CChatAppDlg::AddMessage(CString text)
{
    CEdit* editHistory = (CEdit*)GetDlgItem(IDC_EDIT_HISTORY);
    if (editHistory == NULL)
        return;

    int length = editHistory->GetWindowTextLength();
    editHistory->SetSel(length, length);
    editHistory->ReplaceSel(text + L"\r\n");
}

void CChatAppDlg::OnBnClickedButtonWindows()
{
    CString message;
    GetDlgItemText(IDC_EDIT_MESSAGE, message);
    if (message.IsEmpty())
        return;

    HWND hServer = ::FindWindow(NULL, SERVER_TITLE);
    if (hServer == NULL)
    {
        MessageBoxW(L"Không tìm thấy Server", L"Lỗi", MB_OK | MB_ICONERROR);
        return;
    }

    COPYDATASTRUCT data = {};
    data.dwData = 1;
    data.cbData = (message.GetLength() + 1) * sizeof(wchar_t);
    data.lpData = (PVOID)(LPCWSTR)message;

    ::SendMessage(hServer, WM_COPYDATA, (WPARAM)m_hWnd, (LPARAM)&data);
    AddMessage(L"[Windows Message] Client: " + message);
    SetDlgItemText(IDC_EDIT_MESSAGE, L"");
}

BOOL CChatAppDlg::ConnectPipe()
{
    CSingleLock lock(&m_pipeLock, TRUE);
    if (m_bStopping)
        return FALSE;
    if (m_hPipe != INVALID_HANDLE_VALUE)
        return TRUE;
    if (m_hPipeThread != NULL)
        return FALSE;

    m_hPipe = CreateFile(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (m_hPipe == INVALID_HANDLE_VALUE)
        return FALSE;

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(m_hPipe, &mode, NULL, NULL))
    {
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
        return FALSE;
    }

    m_hPipeThread = CreateThread(NULL, 0, PipeThreadProc, this, 0, NULL);
    if (m_hPipeThread == NULL)
    {
        CloseHandle(m_hPipe);
        m_hPipe = INVALID_HANDLE_VALUE;
        return FALSE;
    }

    return TRUE;
}

DWORD WINAPI CChatAppDlg::PipeThreadProc(LPVOID pParam)
{
    CChatAppDlg* dlg = reinterpret_cast<CChatAppDlg*>(pParam);
    dlg->PipeThread();
    return 0;
}

void CChatAppDlg::PipeThread()
{
    HANDLE pipe = INVALID_HANDLE_VALUE;
    {
        CSingleLock lock(&m_pipeLock, TRUE);
        pipe = m_hPipe;
    }

    while (pipe != INVALID_HANDLE_VALUE)
    {
        CString message;
        DWORD error = ERROR_MORE_DATA;

        while (error == ERROR_MORE_DATA)
        {
            wchar_t buffer[1024] = {};
            DWORD bytesRead = 0;
            error = ReadPipePart(pipe, buffer, sizeof(buffer), &bytesRead);

            if (error != ERROR_SUCCESS && error != ERROR_MORE_DATA)
                break;
            if (bytesRead == 0 || bytesRead % sizeof(wchar_t) != 0)
            {
                error = ERROR_INVALID_DATA;
                break;
            }

            message.Append(buffer, (int)(bytesRead / sizeof(wchar_t)));
        }

        if (error != ERROR_SUCCESS)
            break;

        if (!message.IsEmpty() && message[message.GetLength() - 1] == L'\0')
            message.Delete(message.GetLength() - 1);

        CString* text = new CString(L"[Named Pipe] Server: " + message);
        if (!PostMessage(WM_PIPE_MESSAGE, 0, reinterpret_cast<LPARAM>(text)))
            delete text;
    }

    {
        CSingleLock lock(&m_pipeLock, TRUE);
        if (m_hPipe == pipe)
            m_hPipe = INVALID_HANDLE_VALUE;
    }

    if (pipe != INVALID_HANDLE_VALUE)
        CloseHandle(pipe);

    if (!m_bStopping)
        PostMessage(WM_PIPE_DISCONNECTED, 0, 0);
}

BOOL CChatAppDlg::WritePipeMessage(const CString& message)
{
    CSingleLock lock(&m_pipeLock, TRUE);
    if (m_hPipe == INVALID_HANDLE_VALUE)
    {
        SetLastError(ERROR_PIPE_NOT_CONNECTED);
        return FALSE;
    }

    DWORD bytesToWrite = (message.GetLength() + 1) * sizeof(wchar_t);
    return WritePipeWithTimeout(m_hPipe, (LPCWSTR)message, bytesToWrite);
}

void CChatAppDlg::OnBnClickedButtonPipe()
{
    CString message;
    GetDlgItemText(IDC_EDIT_MESSAGE, message);
    if (message.IsEmpty())
        return;

    if (!ConnectPipe())
    {
        MessageBoxW(L"Không kết nối được Named Pipe!", L"Lỗi", MB_OK | MB_ICONERROR);
        return;
    }

    if (!WritePipeMessage(message))
    {
        DWORD error = GetLastError();
        if (error == ERROR_TIMEOUT)
            MessageBoxW(L"Named Pipe không phản hồi (quá 2 giây).", L"Lỗi", MB_OK | MB_ICONERROR);
        else
            MessageBoxW(L"Gửi bằng Named Pipe thất bại!", L"Lỗi", MB_OK | MB_ICONERROR);
        return;
    }

    AddMessage(L"[Named Pipe] Client: " + message);
    SetDlgItemText(IDC_EDIT_MESSAGE, L"");
}