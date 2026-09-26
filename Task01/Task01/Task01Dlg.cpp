
// Task01Dlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "Task01.h"
#include "Task01Dlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Comdlg32.lib")

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


// CTask01Dlg dialog



CTask01Dlg::CTask01Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_TASK01_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CTask01Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT_CONTENT, m_editContent);
	DDX_Control(pDX, IDC_EDIT_FOLDER, m_Folder);
}

BEGIN_MESSAGE_MAP(CTask01Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_CHOOSE_FOLDER, &CTask01Dlg::OnBnClickedButtonChooseFolder)
	ON_BN_CLICKED(IDC_BUTTON_SAVE, &CTask01Dlg::OnBnClickedButtonSave)
	ON_BN_CLICKED(IDC_BUTTON_LOAD, &CTask01Dlg::OnBnClickedButtonLoad)
END_MESSAGE_MAP()


// CTask01Dlg message handlers

BOOL CTask01Dlg::OnInitDialog()
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
	m_editContent.SetLimitText(0x7FFFFFFE);
	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CTask01Dlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CTask01Dlg::OnPaint()
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
HCURSOR CTask01Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CTask01Dlg::OnBnClickedButtonChooseFolder()
{
	IFileDialog* pDialog = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDialog)); // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cocreateinstance
	if (FAILED(hr)) {
		return;
	}
	DWORD dwOptions = 0;
	pDialog->GetOptions(&dwOptions); // https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/ne-shobjidl_core-_fileopendialogoptions
	pDialog->SetOptions(dwOptions | FOS_PICKFOLDERS);  
	hr = pDialog->Show(m_hWnd);
	if (SUCCEEDED(hr)) {
		IShellItem* pItem = nullptr; 
		hr = pDialog->GetResult(&pItem); // https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-ifiledialog-getresult
		if (SUCCEEDED(hr)) {
			PWSTR pszFolder = nullptr;
			hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFolder); 
			if (SUCCEEDED(hr)) {
				HWND hEditFolder = ::GetDlgItem(m_hWnd, IDC_EDIT_FOLDER);
				::SetWindowTextW(hEditFolder, pszFolder);
				CoTaskMemFree(pszFolder); // https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cotaskmemfree
			}
		}
		pItem->Release();
	}
	pDialog->Release();
}


void CTask01Dlg::OnBnClickedButtonSave()
{
	wchar_t szFolder[MAX_PATH] = {};
	HWND hEditFolder = ::GetDlgItem(m_hWnd, IDC_EDIT_FOLDER);
	::GetWindowTextW(hEditFolder, szFolder, MAX_PATH);
	wchar_t szFile[MAX_PATH] = {};
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = m_hWnd;
	ofn.lpstrFilter = L"Text File (*.txt)\0*.txt\0\0";
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = L"txt";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT; 
	// https://learn.microsoft.com/en-us/windows/win32/api/commdlg/ns-commdlg-openfilenamew
	if (!GetSaveFileNameW(&ofn)) {
		// https://learn.microsoft.com/en-us/windows/win32/api/commdlg/nf-commdlg-getsavefilenamew
		return;
	}
	HWND heditContent = ::GetDlgItem(m_hWnd, IDC_EDIT_CONTENT);
	int nLength = ::GetWindowTextLengthW(heditContent);
	wchar_t* pBuffer = new wchar_t[nLength + 1];
	::GetWindowTextW(heditContent, pBuffer, nLength + 1);
	HANDLE hFile = CreateFileW(szFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew
	if (hFile == INVALID_HANDLE_VALUE) {
		delete[] pBuffer;
		return;
	}
	DWORD dwWritten = 0;
	WriteFile(hFile, pBuffer, nLength * sizeof(wchar_t), &dwWritten, nullptr);
	// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile
	CloseHandle(hFile);
	delete[] pBuffer;
}


void CTask01Dlg::OnBnClickedButtonLoad()
{
	wchar_t szFolder[MAX_PATH] = {};
	HWND hEditFolder = ::GetDlgItem(m_hWnd, IDC_EDIT_FOLDER);
	:: GetWindowTextW(hEditFolder, szFolder, MAX_PATH);
	wchar_t szFile[MAX_PATH] = {};
	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = m_hWnd;
	ofn.lpstrFilter = L"Text File (*.txt)\0*.txt\0\0";
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrInitialDir = szFolder;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	// https://learn.microsoft.com/en-us/windows/win32/api/commdlg/nf-commdlg-getopenfilenamew
	if (!GetOpenFileNameW(&ofn)) {
		return;
	}
	HANDLE hFile = CreateFileW(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return;
	}
	DWORD dwSize = GetFileSize(hFile, NULL);
	// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfilesize
	if (dwSize == INVALID_FILE_SIZE) {
		CloseHandle(hFile);
		return;
	}
	wchar_t* pBuffer = new wchar_t[dwSize / sizeof(wchar_t) + 1];
	DWORD dwRead = 0;
	BOOL bRead = ReadFile(hFile, pBuffer, dwSize, &dwRead, NULL);
	// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile
	CloseHandle(hFile);
	if (!bRead) {
		delete[] pBuffer;
		return;
	}
	pBuffer[dwRead / sizeof(wchar_t)] = L'\0';
	HWND hEditContent = ::GetDlgItem(m_hWnd, IDC_EDIT_CONTENT);
	::SetWindowTextW(hEditContent, pBuffer);
	delete[] pBuffer;
}