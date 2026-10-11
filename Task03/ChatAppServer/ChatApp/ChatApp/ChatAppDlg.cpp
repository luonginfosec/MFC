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
    const DWORD PIPE_WRITE_TIMEOUT_MS = 10000;

    DWORD ReadPipePart(HANDLE pipe, wchar_t* buffer, DWORD bufferSize, DWORD& bytesRead)
    {
        OVERLAPPED ov = {};
        ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        if (ov.hEvent == NULL)
            return GetLastError();

        bytesRead = 0;
        BOOL result = ReadFile(pipe, buffer, bufferSize, NULL, &ov);
        DWORD error = ERROR_SUCCESS;

        if (!result)
        {
            error = GetLastError();

            if (error == ERROR_IO_PENDING)
            {
                result = GetOverlappedResult(pipe, &ov, &bytesRead, TRUE);

                if (result)
                    error = ERROR_SUCCESS;
                else
                    error = GetLastError();
            }
        }
        else
        {
            if (!GetOverlappedResult(pipe, &ov, &bytesRead, FALSE))
                error = GetLastError();
        }

        if (error == ERROR_MORE_DATA && bytesRead == 0)
            bytesRead = bufferSize;

        CloseHandle(ov.hEvent);
        return error;
    }

    BOOL ReadPipeMessage(HANDLE pipe, CString& message)
    {
        message.Empty();

        while (TRUE)
        {
            wchar_t buffer[1024] = {};
            DWORD bytesRead = 0;
            DWORD error = ReadPipePart(pipe, buffer, sizeof(buffer), bytesRead);

            if (error != ERROR_SUCCESS && error != ERROR_MORE_DATA)
            {
                SetLastError(error);
                return FALSE;
            }

            if (bytesRead == 0)
            {
                SetLastError(ERROR_BROKEN_PIPE);
                return FALSE;
            }

            if (bytesRead % sizeof(wchar_t) != 0)
            {
                SetLastError(ERROR_INVALID_DATA);
                return FALSE;
            }

            message.Append(buffer, (int)(bytesRead / sizeof(wchar_t)));

            if (error == ERROR_SUCCESS)
            {
                int length = message.GetLength();

                if (length > 0 && message[length - 1] == L'\0')
                    message.Truncate(length - 1);

                return TRUE;
            }
        }
    }

    BOOL WritePipeWithTimeout(HANDLE pipe, const void* buffer, DWORD bytesToWrite, DWORD* bytesWritten)
    {
        OVERLAPPED ov = {};
        ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

        if (ov.hEvent == NULL)
            return FALSE;

        *bytesWritten = 0;
        BOOL result = WriteFile(pipe, buffer, bytesToWrite, NULL, &ov);
        DWORD error = ERROR_SUCCESS;

        if (!result)
        {
            error = GetLastError();

            if (error == ERROR_IO_PENDING)
            {
                DWORD wait = WaitForSingleObject(ov.hEvent, PIPE_WRITE_TIMEOUT_MS);

                if (wait == WAIT_OBJECT_0)
                {
                    result = GetOverlappedResult(pipe, &ov, bytesWritten, FALSE);

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
        else
        {
            result = GetOverlappedResult(pipe, &ov, bytesWritten, FALSE);

            if (!result)
                error = GetLastError();
        }

        if (result && *bytesWritten != bytesToWrite)
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
    ON_WM_DESTROY()
    ON_WM_COPYDATA()
    ON_MESSAGE(WM_PIPE_MESSAGE, &CChatAppDlg::OnPipeMessage)
    ON_BN_CLICKED(IDC_BUTTON_WINDOWS, &CChatAppDlg::OnBnClickedButtonWindows)
    ON_BN_CLICKED(IDC_BUTTON_PIPE, &CChatAppDlg::OnBnClickedButtonPipe)
END_MESSAGE_MAP()

BOOL CChatAppDlg::OnInitDialog()
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

    CEdit* editMessage = (CEdit*)GetDlgItem(IDC_EDIT_MESSAGE);
    CEdit* editHistory = (CEdit*)GetDlgItem(IDC_EDIT_HISTORY);

    if (editMessage != NULL)
        editMessage->SetLimitText(0);

    if (editHistory != NULL)
        editHistory->SetLimitText(0);

    m_hPipeThread = CreateThread(NULL, 0, PipeThreadProc, this, 0, NULL);

    if (m_hPipeThread == NULL)
        MessageBoxW(L"Không tạo được Pipe Thread!", L"Lỗi", MB_OK | MB_ICONERROR);

    return TRUE;
}

void CChatAppDlg::OnDestroy()
{
    {
        CSingleLock lock(&m_pipeLock, TRUE);
        m_bStopping = TRUE;
    }

    if (m_hPipeThread != NULL)
    {
        while (WaitForSingleObject(m_hPipeThread, 100) == WAIT_TIMEOUT)
        {
            CSingleLock lock(&m_pipeLock, TRUE);

            if (m_hPipe != INVALID_HANDLE_VALUE)
                CancelIoEx(m_hPipe, NULL);
        }

        CloseHandle(m_hPipeThread);
        m_hPipeThread = NULL;
    }

    MSG msg = {};

    while (::PeekMessage(&msg, m_hWnd, WM_PIPE_MESSAGE, WM_PIPE_MESSAGE, PM_REMOVE))
        delete (CString*)msg.lParam;

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

void CChatAppDlg::AddMessage(CString text)
{
    CEdit* edit = (CEdit*)GetDlgItem(IDC_EDIT_HISTORY);

    if (edit == NULL)
        return;

    int len = edit->GetWindowTextLength();
    edit->SetSel(len, len);
    edit->ReplaceSel(text + L"\r\n");
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

BOOL CChatAppDlg::OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct)
{
    if (pCopyDataStruct == NULL)
        return FALSE;

    if (pCopyDataStruct->lpData == NULL)
        return FALSE;

    if (pCopyDataStruct->cbData < sizeof(wchar_t))
        return FALSE;

    if (pCopyDataStruct->cbData % sizeof(wchar_t) != 0)
        return FALSE;

    LPCWSTR data = (LPCWSTR)pCopyDataStruct->lpData;
    int length = (int)(pCopyDataStruct->cbData / sizeof(wchar_t));

    if (data[length - 1] == L'\0')
        length--;

    CString message(data, length);
    CString display = L"[Windows Message] Client: ";
    display += message;
    AddMessage(display);

    return TRUE;
}

void CChatAppDlg::OnBnClickedButtonWindows()
{
    CString message;
    GetDlgItemText(IDC_EDIT_MESSAGE, message);

    if (message.IsEmpty())
        return;

    HWND hClient = ::FindWindow(NULL, CLIENT_TITLE);

    if (hClient == NULL)
    {
        MessageBoxW(L"Không tìm thấy Client!", L"Lỗi", MB_OK | MB_ICONERROR);
        return;
    }

    COPYDATASTRUCT data = {};
    data.dwData = 1;
    data.cbData = (message.GetLength() + 1) * sizeof(wchar_t);
    data.lpData = (PVOID)(LPCWSTR)message;

    ::SendMessage(hClient, WM_COPYDATA, (WPARAM)m_hWnd, (LPARAM)&data);

    CString display = L"[Windows Message] Server: ";
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
    while (TRUE)
    {
        {
            CSingleLock lock(&m_pipeLock, TRUE);

            if (m_bStopping)
                return;
        }

        HANDLE pipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);

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

        OVERLAPPED ov = {};
        ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        BOOL connected = FALSE;

        if (ov.hEvent != NULL)
        {
            connected = ConnectNamedPipe(pipe, &ov);

            if (!connected)
            {
                DWORD error = GetLastError();

                if (error == ERROR_PIPE_CONNECTED)
                {
                    connected = TRUE;
                }
                else if (error == ERROR_IO_PENDING)
                {
                    DWORD wait = WaitForSingleObject(ov.hEvent, INFINITE);

                    if (wait == WAIT_OBJECT_0)
                    {
                        DWORD ignored = 0;
                        connected = GetOverlappedResult(pipe, &ov, &ignored, FALSE);
                    }
                    else
                    {
                        CancelIoEx(pipe, &ov);
                        WaitForSingleObject(ov.hEvent, INFINITE);
                    }
                }
            }

            CloseHandle(ov.hEvent);
        }

        if (connected)
        {
            CSingleLock lock(&m_pipeLock, TRUE);

            if (!m_bStopping)
                m_bPipeConnected = TRUE;
        }

        while (connected)
        {
            {
                CSingleLock lock(&m_pipeLock, TRUE);

                if (m_bStopping)
                    break;
            }

            CString message;

            if (!ReadPipeMessage(pipe, message))
                break;

            {
                CSingleLock lock(&m_pipeLock, TRUE);

                if (m_bStopping)
                    break;
            }

            CString display = L"[Named Pipe] Client: ";
            display += message;
            CString* text = new CString(display);

            if (!PostMessage(WM_PIPE_MESSAGE, 0, (LPARAM)text))
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
    BOOL result = FALSE;
    DWORD error = ERROR_PIPE_NOT_CONNECTED;

    {
        CSingleLock lock(&m_pipeLock, TRUE);

        if (m_hPipe != INVALID_HANDLE_VALUE && m_bPipeConnected)
        {
            DWORD bytesWritten = 0;
            result = WritePipeWithTimeout(m_hPipe, (LPCWSTR)message, (message.GetLength() + 1) * sizeof(wchar_t), &bytesWritten);

            if (!result)
                error = GetLastError();
        }
    }

    if (!result)
        SetLastError(error);

    return result;
}

void CChatAppDlg::OnBnClickedButtonPipe()
{
    CString message;
    GetDlgItemText(IDC_EDIT_MESSAGE, message);

    if (message.IsEmpty())
        return;

    if (!WritePipeMessage(message))
    {
        DWORD error = GetLastError();

        if (error == ERROR_TIMEOUT)
            MessageBoxW(L"Named Pipe không phản hồi (quá 10 giây).", L"Lỗi", MB_OK | MB_ICONERROR);
        else
            MessageBoxW(L"Client chưa kết nối hoặc gửi Named Pipe thất bại!", L"Lỗi", MB_OK | MB_ICONERROR);

        return;
    }

    CString display = L"[Named Pipe] Server: ";
    display += message;
    AddMessage(display);
    SetDlgItemText(IDC_EDIT_MESSAGE, L"");
}