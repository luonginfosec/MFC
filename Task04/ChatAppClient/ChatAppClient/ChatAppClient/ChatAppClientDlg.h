
// ChatAppClientDlg.h : header file
//

#pragma once

// Bộ đệm byte động cấp phát bằng Heap API (không dùng STL)
struct ByteBuffer
{
	BYTE* data = nullptr;		// Vùng nhớ
	SIZE_T size = 0;			// Số byte đang dùng
	SIZE_T capacity = 0;		// Số byte đã cấp phát
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
	SOCKET m_socket = INVALID_SOCKET;	// Socket kết nối tới server
	bool m_started = false;				// WSAStartup thành công
	bool m_connected = false;			// Đã kết nối TCP xong
	ByteBuffer m_received;				// Dữ liệu nhận chưa đủ thành 1 tin
	ByteBuffer m_pending;				// Gói tin đang gửi (header + nội dung)
	SIZE_T m_sent = 0;					// Số byte của m_pending đã gửi
	WCHAR* m_message = nullptr;			// Nội dung tin đang gửi (để hiển thị khi gửi xong)

	void AddMessage(LPCWSTR prefix, LPCWSTR text);
	void Stop(LPCWSTR text);
	void Connected();
	void Receive();
	bool ParseMessages();
	void Flush();
	void EnableItem(int id, BOOL enable);
};
