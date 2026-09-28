
// Task02Dlg.h : header file
//

#pragma once
#include <vector>

// CTask02Dlg dialog
class CTask02Dlg : public CDialogEx
{
// Construction
public:
	CTask02Dlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TASK02_DIALOG };
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
	DECLARE_MESSAGE_MAP()
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnOK();

private:
	CTreeCtrl m_treeContacts;
	CListCtrl m_listMessages;
	CEdit m_editMessage;

	void AddMessage(const CString& content, bool sentByMe);

	struct Message
	{
		CString content;
		bool sentByMe;
	};

	std::vector<Message> m_history[9999];

	int m_currentContact = -1;

	void ShowHistory();
public:
	afx_msg void OnBnClickedButtonSend();
	afx_msg void OnBnClickedButtonExit();
	afx_msg void OnTvnSelchangedTreeContacts(NMHDR* pNMHDR, LRESULT* pResult);
};
