#pragma once

class CChatAppClientDlg : public CDialogEx
{
public:
	CChatAppClientDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CHATAPPCLIENT_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);

	HICON m_hIcon;

	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDestroy();
	afx_msg LRESULT OnChatRecv(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnChatClosed(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedButtonConnect();
	afx_msg void OnBnClickedButtonSend();
	afx_msg void OnBnClickedCheckAutorun();

private:
	SOCKET m_sock = INVALID_SOCKET;

	static UINT RecvThread(LPVOID param);
	void AddMessage(const CString& text);
	void Disconnect(const CString& text);
};
