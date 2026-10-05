
// ChatAppDlg.h : header file
//

#pragma once

#include <afxmt.h>


// CChatAppDlg dialog
class CChatAppDlg : public CDialogEx
{
// Construction
public:
	CChatAppDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CHATAPP_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();	
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	afx_msg BOOL OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
	afx_msg LRESULT OnPipeMessage(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnPipeDisconnected(WPARAM wParam, LPARAM lParam);
	void AddMessage(CString text);

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedButtonWindows();
	afx_msg void OnBnClickedButtonPipe();
private:
	HANDLE m_hPipe;
	HANDLE m_hPipeThread;
	BOOL m_bStopping;
	CCriticalSection m_pipeLock;
	BOOL ConnectPipe();
	BOOL WritePipeMessage(const CString& message);
	static DWORD WINAPI PipeThreadProc(LPVOID pParam);
	void PipeThread();
};
