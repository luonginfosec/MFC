// CLoginDlg.cpp : implementation file
//

#include "pch.h"
#include "Task02.h"
#include "CLoginDlg.h"
#include "afxdialogex.h"


// CLoginDlg dialog

IMPLEMENT_DYNAMIC(CLoginDlg, CDialogEx)

CLoginDlg::CLoginDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_LOGIN_DIALOG, pParent)
{
    
	m_accounts[L"luongvd"] = L"luongvd";
   
}

BOOL CLoginDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetDlgItemTextW(IDC_EDIT_USERNAME, L"luongvd");
    SetDlgItemTextW(IDC_EDIT_PASSWORD, L"luongvd");
    return TRUE;
}

CLoginDlg::~CLoginDlg()
{

}

void CLoginDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CLoginDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_LOGIN, &CLoginDlg::OnBnClickedButtonLogin)
	ON_BN_CLICKED(IDC_BUTTON_REGISTER, &CLoginDlg::OnBnClickedButtonRegister)
	ON_BN_CLICKED(IDC_BUTTON_EXIT_1, &CLoginDlg::OnBnClickedButtonExit1)
END_MESSAGE_MAP()

void CLoginDlg::OnOK()
{
    OnBnClickedButtonLogin();
}

bool CLoginDlg::ReadInput(CString& username, CString& password)
{
    GetDlgItemText(IDC_EDIT_USERNAME, username);
    GetDlgItemText(IDC_EDIT_PASSWORD, password);
    username.Trim();
    if (username.IsEmpty() || password.IsEmpty())
    {
        MessageBoxW(L"Nhập đầy đủ tên đăng nhập và mật khẩu", L"Lỗi", MB_OK | MB_ICONERROR);
        return false;
    }
    return true;
}


void CLoginDlg::OnBnClickedButtonLogin()
{
    CString username, password;
    if (!ReadInput(username, password))
        return;
    auto account = m_accounts.find(username);
    if (account == m_accounts.end()) {
        MessageBoxW(L"Tài khoản không tồn tại", L"Lỗi", MB_OK | MB_ICONERROR);
        return;
    }
    if (account->second != password)
    {
        MessageBoxW(L"Sai mật khẩu", L"Lỗi", MB_OK | MB_ICONERROR);
        return;
    }
    m_username = username;
    EndDialog(IDOK);
}


void CLoginDlg::OnBnClickedButtonRegister()
{
    CString username, password;
    if (!ReadInput(username, password)) {
        return;
    }
    m_accounts[username] = password;
    MessageBoxW(L"Đăng ký thành công bấm đăng nhập để tiếp tục!", L"Thành công", MB_OK | MB_ICONINFORMATION);
}


void CLoginDlg::OnBnClickedButtonExit1()
{
    EndDialog(IDCANCEL);
}
