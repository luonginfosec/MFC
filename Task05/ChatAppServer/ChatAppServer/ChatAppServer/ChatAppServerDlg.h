
// ChatAppServerDlg.h : header file
//

#pragma once

struct ByteBuffer
{
	BYTE* data = nullptr;
	SIZE_T size = 0;
	SIZE_T capacity = 0;
};

// CChatAppServerDlg dialog
class CChatAppServerDlg : public CDialogEx
{
// Construction
public:
	CChatAppServerDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CHATAPPSERVER_DIALOG };
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
	afx_msg void OnBnClickedButtonListen();
	afx_msg void OnBnClickedButtonSend();
	afx_msg void OnBnClickedCheckAutorun();

private:
	SOCKET m_listen = INVALID_SOCKET;
	SOCKET m_client = INVALID_SOCKET;
	bool m_started = false;
	ByteBuffer m_received;
	ByteBuffer m_pending;
	SIZE_T m_sent = 0;
	WCHAR* m_message = nullptr;

	void AddMessage(LPCWSTR prefix, LPCWSTR text);
	void ShowAddresses(UINT port);
	void StopListen(LPCWSTR text);
	void CloseClient(LPCWSTR text);
	void Accept();
	void Receive();
	bool ParseMessages();
	void Flush();
	void EnableItem(int id, BOOL enable);
};
