
// Task02Dlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "Task02.h"
#include "Task02Dlg.h"
#include "afxdialogex.h"
#include "CLoginDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
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


// CTask02Dlg dialog



CTask02Dlg::CTask02Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TASK02_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTask02Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE_CONTACTS, m_treeContacts);
	DDX_Control(pDX, IDC_LIST_MESSAGES, m_listMessages);
	DDX_Control(pDX, IDC_EDIT_MESSAGE, m_editMessage);
}

BEGIN_MESSAGE_MAP(CTask02Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_SEND, &CTask02Dlg::OnBnClickedButtonSend)
	ON_BN_CLICKED(IDC_BUTTON_EXIT, &CTask02Dlg::OnBnClickedButtonExit)
	ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_CONTACTS, &CTask02Dlg::OnTvnSelchangedTreeContacts)
END_MESSAGE_MAP()


// CTask02Dlg message handlers

BOOL CTask02Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	CLoginDlg login(this);
	INT_PTR result = login.DoModal();
	if (result != IDOK)
	{
		if (result == -1)
			MessageBoxW(L"Không tạo được dialog đăng nhập", L"Lỗi", MB_OK | MB_ICONERROR);
		EndDialog(IDCANCEL);
		return TRUE;
	}
	SetDlgItemText(IDC_STATIC_USER, L"Tài khoản: " + login.m_username);

	HTREEITEM hRootFriends = m_treeContacts.InsertItem(L"Bạn bè");
	HTREEITEM hAn = m_treeContacts.InsertItem(L"An", hRootFriends);
	HTREEITEM hBinh = m_treeContacts.InsertItem(L"Bình", hRootFriends);
	

	HTREEITEM hRoot = m_treeContacts.InsertItem(L"Chí cốt");
	HTREEITEM hLuong = m_treeContacts.InsertItem(L"An", hRoot);
	HTREEITEM hAnh = m_treeContacts.InsertItem(L"Bình", hRoot);

	m_treeContacts.SetItemData(hAn, 1);
	m_treeContacts.SetItemData(hBinh, 2);
	m_treeContacts.SetItemData(hLuong, 3);
	m_treeContacts.SetItemData(hAnh, 4);

	m_treeContacts.Expand(hRootFriends, TVE_EXPAND); // TVE_EXPAND mở rộng nhánh không ẩn. 
	m_treeContacts.Expand(hRoot, TVE_EXPAND); // TVE_EXPAND mở rộng nhánh không ẩn. 

	// https://learn.microsoft.com/en-us/windows/win32/controls/tvm-expand
	m_listMessages.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES); // cho phép chọn cả dòng , hiện thị đường kẻ 
	// https://learn.microsoft.com/en-us/windows/win32/controls/extended-list-view-styles
	CRect rect;
	// https://learn.microsoft.com/en-us/cpp/atl-mfc-shared/reference/crect-class?view=msvc-170
	m_listMessages.GetClientRect(&rect);
	int columnWidth = (rect.Width() - GetSystemMetrics(SM_CXVSCROLL) - 4) / 2;
	// https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getsystemmetrics
	m_listMessages.InsertColumn(0, L"Người nhận", LVCFMT_LEFT, columnWidth);
	m_listMessages.InsertColumn(1, L"Người gửi", LVCFMT_RIGHT, columnWidth);
	// https://learn.microsoft.com/en-us/cpp/mfc/reference/clistctrl-class?view=msvc-170#insertcolumn
	m_history[0].push_back({L"Chào bạn! Mình là An.", false});
	m_history[0].push_back({L"Chào An!",true});
	m_history[1].push_back({L"Chào bạn! Mình là Bình.",false});
	m_history[2].push_back({ L"Chào chí cốt! Đi chơi không",false });
	m_history[2].push_back({ L"Ok! Dắt xe ra đi.",true });
	m_history[3].push_back({ L"Chào bạn! Mình là Anh.",false });
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CTask02Dlg::OnSysCommand(UINT nID, LPARAM lParam)
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

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CTask02Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CTask02Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTask02Dlg::OnOK()
{
}

BOOL CTask02Dlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN && pMsg->hwnd == m_editMessage.GetSafeHwnd() && (::GetKeyState(VK_CONTROL) & 0x8000)) {
		OnBnClickedButtonSend();
		return TRUE;
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}

void CTask02Dlg::ShowHistory()
{
	m_listMessages.DeleteAllItems();

	if (m_currentContact == -1)
		return;

	for (const Message& message : m_history[m_currentContact])
	{
		AddMessage(message.content, message.sentByMe);
	}
}

void CTask02Dlg::AddMessage(const CString& content, bool sentByMe)
{
	CString text = content;
	text.Replace(L"\r\n", L"\n");
	text.Replace(L"\r", L"\n");
	int start = 0;
	while (true)
	{
		int end = text.Find(L'\n', start);
		CString line;
		if (end == -1)
			line = text.Mid(start);
		else
			line = text.Mid(start, end - start);
		int row = m_listMessages.InsertItem(m_listMessages.GetItemCount(), L"");
		int column = 0;
		if (sentByMe) {
			column = 1;
		}
		else {
			column = 0;
		}
		m_listMessages.SetItemText(row, column, line);
		if (end == -1)
			break;
		start = end + 1;
	}
	m_listMessages.EnsureVisible(m_listMessages.GetItemCount() - 1, FALSE);
}

void CTask02Dlg::OnBnClickedButtonSend()
{
	if (m_currentContact == -1)
		return;

	CString content;
	m_editMessage.GetWindowTextW(content);

	CString check = content;
	check.Trim();

	if (check.IsEmpty())
		return;

	auto& history = m_history[m_currentContact];

	history.push_back({ content, true });

	AddMessage(content, true);

	CString reply = L"Mình đã nhận được tin nhắn của bạn.";

	history.push_back({ reply, false });
	AddMessage(reply, false);

	m_editMessage.SetWindowTextW(L"");
	m_editMessage.SetFocus();
}

void CTask02Dlg::OnTvnSelchangedTreeContacts(NMHDR* pNMHDR, LRESULT* pResult)
{
	auto pTree = reinterpret_cast<NMTREEVIEW*>(pNMHDR);

	HTREEITEM hItem = pTree->itemNew.hItem;

	DWORD_PTR id = 0;

	if (hItem != nullptr)
		id = m_treeContacts.GetItemData(hItem);

	if (id == 1)
		m_currentContact = 0;
	else if (id == 2)
		m_currentContact = 1;
	else if (id == 3)
		m_currentContact = 2;
	else if (id == 4)
		m_currentContact = 3;
	else
		m_currentContact = -1;

	ShowHistory();

	BOOL canSend = (m_currentContact != -1);

	m_editMessage.EnableWindow(canSend);
	GetDlgItem(IDC_BUTTON_SEND)->EnableWindow(canSend);

	*pResult = 0;
}

void CTask02Dlg::OnBnClickedButtonExit()
{
	EndDialog(IDCANCEL);
}


