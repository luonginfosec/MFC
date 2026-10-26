
// ChatAppClientDlg.h : header file
//

#pragma once

struct ByteBuffer
{
	BYTE* data = nullptr;
	SIZE_T size = 0;
	SIZE_T capacity = 0;
};

// CChatAppClientDlg dialog
class CChatAppClientDlg : public CDialogEx
{
// Construction
public:
	CChatAppClientDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CHATAPPCLIENT_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg LRESULT OnSocket(WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonConnect();
	afx_msg void OnBnClickedButtonSend();

private:
	SOCKET m_socket = INVALID_SOCKET;
	bool m_started = false;
	bool m_connected = false;
	ByteBuffer m_received;
	ByteBuffer m_pending;
	SIZE_T m_sent = 0;
	WCHAR* m_message = nullptr;

	void AddMessage(LPCWSTR prefix, LPCWSTR text);
	void Stop(LPCWSTR text);
	void Connected();
	void Receive();
	bool ParseMessages();
	void Flush();
	void EnableItem(int id, BOOL enable);
};
