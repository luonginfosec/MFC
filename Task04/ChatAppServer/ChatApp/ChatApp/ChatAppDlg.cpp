#include "pch.h"
#include "ChatApp.h"
#include "ChatAppDlg.h"
#include <climits>
#include <cstring>
#include <exception>

const UINT WM_SOCKET = WM_APP + 1;
const bool IS_SERVER = true;

BEGIN_MESSAGE_MAP(CChatAppDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CChatAppDlg::OnConnect)
    ON_BN_CLICKED(IDC_BUTTON_SEND, &CChatAppDlg::OnSend)
    ON_MESSAGE(WM_SOCKET, &CChatAppDlg::OnSocket)
    ON_WM_DESTROY()
END_MESSAGE_MAP()

CChatAppDlg::CChatAppDlg(CWnd* parent) : CDialogEx(IDD_CHATAPP_DIALOG, parent)
{
}

BOOL CChatAppDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    HICON icon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    SetIcon(icon, TRUE);
    SetIcon(icon, FALSE);
    SetDlgItemText(IDC_EDIT_PORT, L"5000");
    SetDlgItemText(IDC_EDIT_IP, L"127.0.0.1");
    if (IS_SERVER)
    {
        SetWindowText(L"Chat TCP - Máy chủ");
        SetDlgItemText(IDC_EDIT_IP, L"0.0.0.0");
        GetDlgItem(IDC_EDIT_IP)->EnableWindow(FALSE);
        SetDlgItemText(IDC_BUTTON_CONNECT, L"Lắng nghe");
    }
    else
        SetWindowText(L"Chat TCP - Máy khách");
    ((CEdit*)GetDlgItem(IDC_EDIT_MESSAGE))->SetLimitText(INT_MAX - 1);
    ((CEdit*)GetDlgItem(IDC_EDIT_HISTORY))->SetLimitText(INT_MAX - 1);
    GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
    WSADATA data = {};
    m_started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    if (!m_started)
    {
        AddMessage(L"Không khởi tạo được Winsock.");
        GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(FALSE);
    }
    return TRUE;
}

void CChatAppDlg::AddMessage(const CString& text)
{
    CEdit* history = (CEdit*)GetDlgItem(IDC_EDIT_HISTORY);
    int length = history->GetWindowTextLength();
    history->SetSel(length, length);
    history->ReplaceSel(text + L"\r\n", FALSE);
}

void CChatAppDlg::Stop(const CString& text)
{
    if (!m_pending.empty())
        AddMessage(L"Tin đang gửi chưa hoàn tất.");
    if (m_socket != INVALID_SOCKET)
    {
        WSAAsyncSelect(m_socket, m_hWnd, 0, 0);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    if (m_listener != INVALID_SOCKET)
    {
        WSAAsyncSelect(m_listener, m_hWnd, 0, 0);
        closesocket(m_listener);
        m_listener = INVALID_SOCKET;
    }
    MSG message = {};
    while (PeekMessage(&message, m_hWnd, WM_SOCKET, WM_SOCKET, PM_REMOVE))
    {
    }
    m_connected = false;
    m_received.clear();
    m_pending.clear();
    m_sent = 0;
    m_message.Empty();
    GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
    GetDlgItem(IDC_EDIT_MESSAGE)->EnableWindow(TRUE);
    GetDlgItem(IDC_EDIT_PORT)->EnableWindow(TRUE);
    if (IS_SERVER)
        SetDlgItemText(IDC_BUTTON_CONNECT, L"Lắng nghe");
    else
    {
        SetDlgItemText(IDC_BUTTON_CONNECT, L"Kết nối");
        GetDlgItem(IDC_EDIT_IP)->EnableWindow(TRUE);
    }
    if (!text.IsEmpty())
        AddMessage(text);
}

void CChatAppDlg::OnConnect()
{
    if (m_socket != INVALID_SOCKET || m_listener != INVALID_SOCKET)
    {
        Stop(L"Đã dừng kết nối.");
        return;
    }
    BOOL valid = FALSE;
    UINT port = GetDlgItemInt(IDC_EDIT_PORT, &valid, FALSE);
    if (!valid || port == 0 || port > 65535)
    {
        AfxMessageBox(L"Nhập cổng từ 1 đến 65535.");
        return;
    }
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons((u_short)port);
    if (IS_SERVER)
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    else
    {
        CString ip;
        GetDlgItemText(IDC_EDIT_IP, ip);
        ip.Trim();
        if (InetPtonW(AF_INET, ip, &address.sin_addr) != 1)
        {
            AfxMessageBox(L"Nhập địa chỉ IPv4 của máy chủ.");
            return;
        }
    }
    SOCKET connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection == INVALID_SOCKET)
    {
        AddMessage(L"Không tạo được socket.");
        return;
    }
    if (IS_SERVER)
    {
        m_listener = connection;
        BOOL exclusive = TRUE;
        int result = setsockopt(connection, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive));
        if (result != SOCKET_ERROR)
            result = bind(connection, (sockaddr*)&address, sizeof(address));
        if (result != SOCKET_ERROR)
            result = listen(connection, 1);
        if (result != SOCKET_ERROR)
            result = WSAAsyncSelect(connection, m_hWnd, WM_SOCKET, FD_ACCEPT);
        if (result == SOCKET_ERROR)
        {
            Stop(L"Không lắng nghe được. Kiểm tra cổng đang bị sử dụng.");
            return;
        }
        AddMessage(L"Đang chờ máy khách kết nối...");
    }
    else
    {
        m_socket = connection;
        if (WSAAsyncSelect(connection, m_hWnd, WM_SOCKET, FD_CONNECT | FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
        {
            Stop(L"Không đăng ký được sự kiện socket.");
            return;
        }
        int result = connect(connection, (sockaddr*)&address, sizeof(address));
        if (result == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
        {
            Stop(L"Không kết nối được. Kiểm tra IP, cổng và tường lửa.");
            return;
        }
        AddMessage(L"Đang kết nối...");
    }
    SetDlgItemText(IDC_BUTTON_CONNECT, L"Ngắt / Dừng");
    GetDlgItem(IDC_EDIT_IP)->EnableWindow(FALSE);
    GetDlgItem(IDC_EDIT_PORT)->EnableWindow(FALSE);
}

void CChatAppDlg::Connected()
{
    m_connected = true;
    GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(TRUE);
    AddMessage(L"Đã kết nối TCP.");
}

LRESULT CChatAppDlg::OnSocket(WPARAM wParam, LPARAM lParam)
{
    SOCKET connection = (SOCKET)wParam;
    if (connection != m_socket && connection != m_listener)
        return 0;
    int event = WSAGETSELECTEVENT(lParam);
    int error = WSAGETSELECTERROR(lParam);
    if (error != 0)
    {
        CString text;
        text.Format(L"Lỗi socket: %d. Kiểm tra IP, cổng và tường lửa.", error);
        Stop(text);
        return 0;
    }
    try
    {
        if (connection == m_listener && event == FD_ACCEPT)
        {
            SOCKET client = accept(m_listener, nullptr, nullptr);
            if (client == INVALID_SOCKET)
                return 0;
            if (m_socket != INVALID_SOCKET)
            {
                closesocket(client);
                return 0;
            }
            m_socket = client;
            if (WSAAsyncSelect(client, m_hWnd, WM_SOCKET, FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
                Stop(L"Không đăng ký được sự kiện socket.");
            else
                Connected();
        }
        else if (connection == m_socket)
        {
            if (event == FD_CONNECT)
                Connected();
            else if (event == FD_READ || event == FD_CLOSE)
                Receive();
            else if (event == FD_WRITE && m_connected)
                Flush();
        }
    }
    catch (const std::exception&)
    {
        Stop(L"Không đủ bộ nhớ để xử lý tin nhắn.");
    }
    return 0;
}

bool CChatAppDlg::ParseMessages()
{
    size_t offset = 0;
    while (m_received.size() - offset >= 4)
    {
        u_long header = 0;
        memcpy(&header, m_received.data() + offset, 4);
        u_long length = ntohl(header);
        if (length == 0 || length > INT_MAX - 1)
        {
            Stop(L"Độ dài tin nhắn không hợp lệ.");
            return false;
        }
        if (m_received.size() - offset - 4 < length)
            break;
        const char* data = m_received.data() + offset + 4;
        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, (int)length, nullptr, 0);
        if (count == 0)
        {
            Stop(L"Tin nhắn không phải UTF-8 hợp lệ.");
            return false;
        }
        CString text;
        wchar_t* buffer = text.GetBuffer(count);
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data, (int)length, buffer, count);
        text.ReleaseBuffer(count);
        AddMessage(L"Bạn bên kia: " + text);
        offset += 4 + (size_t)length;
    }
    m_received.erase(0, offset);
    return true;
}

void CChatAppDlg::Receive()
{
    char buffer[8192];
    while (m_socket != INVALID_SOCKET)
    {
        int count = recv(m_socket, buffer, sizeof(buffer), 0);
        if (count > 0)
        {
            m_received.append(buffer, count);
            if (!ParseMessages())
                return;
        }
        else if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
            return;
        else
        {
            if (!m_received.empty())
                Stop(L"Mất kết nối khi tin nhắn chưa nhận đủ.");
            else
                Stop(L"Kết nối đã đóng. Bấm nút để kết nối lại.");
            return;
        }
    }
}

void CChatAppDlg::OnSend()
{
    if (!m_connected || !m_pending.empty())
        return;
    GetDlgItemText(IDC_EDIT_MESSAGE, m_message);
    if (m_message.IsEmpty())
        return;
    int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, m_message, m_message.GetLength(), nullptr, 0, nullptr, nullptr);
    if (length <= 0 || length > INT_MAX - 1)
    {
        AfxMessageBox(L"Không mã hóa được tin nhắn.");
        return;
    }
    try
    {
        m_pending.resize(4 + (size_t)length);
        u_long header = htonl((u_long)length);
        memcpy(&m_pending[0], &header, 4);
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, m_message, m_message.GetLength(), &m_pending[4], length, nullptr, nullptr);
        m_sent = 0;
        GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(FALSE);
        GetDlgItem(IDC_EDIT_MESSAGE)->EnableWindow(FALSE);
        Flush();
    }
    catch (const std::exception&)
    {
        Stop(L"Không đủ bộ nhớ để gửi tin nhắn.");
    }
}

void CChatAppDlg::Flush()
{
    if (m_pending.empty())
        return;
    while (m_sent < m_pending.size())
    {
        size_t remaining = m_pending.size() - m_sent;
        int length = 8192;
        if (remaining < (size_t)length)
            length = (int)remaining;
        int count = send(m_socket, m_pending.data() + m_sent, length, 0);
        if (count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
            return;
        if (count <= 0)
        {
            Stop(L"Gửi thất bại. Kết nối đã đóng.");
            return;
        }
        m_sent += count;
    }
    AddMessage(L"Tôi: " + m_message);
    m_pending.clear();
    m_message.Empty();
    m_sent = 0;
    SetDlgItemText(IDC_EDIT_MESSAGE, L"");
    GetDlgItem(IDC_EDIT_MESSAGE)->EnableWindow(TRUE);
    GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(TRUE);
    GetDlgItem(IDC_EDIT_MESSAGE)->SetFocus();
}

void CChatAppDlg::OnOK()
{
}

BOOL CChatAppDlg::PreTranslateMessage(MSG* message)
{
    if (message->message == WM_KEYDOWN && message->wParam == VK_RETURN && GetKeyState(VK_CONTROL) < 0)
    {
        OnSend();
        return TRUE;
    }
    return CDialogEx::PreTranslateMessage(message);
}

void CChatAppDlg::OnDestroy()
{
    Stop(L"");
    if (m_started)
        WSACleanup();
    CDialogEx::OnDestroy();
}
