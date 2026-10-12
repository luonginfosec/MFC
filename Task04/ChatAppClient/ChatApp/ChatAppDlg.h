#pragma once
#include "afxdialogex.h"
#include <string>

class CChatAppDlg : public CDialogEx
{
public:
    CChatAppDlg(CWnd* parent = nullptr);
protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    BOOL PreTranslateMessage(MSG* message) override;
    afx_msg void OnConnect();
    afx_msg void OnSend();
    afx_msg void OnDestroy();
    afx_msg LRESULT OnSocket(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()
private:
    SOCKET m_socket = INVALID_SOCKET;
    SOCKET m_listener = INVALID_SOCKET;
    bool m_started = false;
    bool m_connected = false;
    std::string m_received;
    std::string m_pending;
    size_t m_sent = 0;
    CString m_message;
    void AddMessage(const CString& text);
    void Stop(const CString& text);
    void Connected();
    void Receive();
    void Flush();
    bool ParseMessages();
};
