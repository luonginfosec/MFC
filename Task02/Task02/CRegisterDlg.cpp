// CRegisterDlg.cpp : implementation file
//

#include "pch.h"
#include "Task02.h"
#include "CRegisterDlg.h"
#include "afxdialogex.h"


// CRegisterDlg dialog

IMPLEMENT_DYNAMIC(CRegisterDlg, CDialogEx)

CRegisterDlg::CRegisterDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_REGISTER_DIALOG, pParent)
{

}

CRegisterDlg::~CRegisterDlg()
{
}

void CRegisterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CRegisterDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_REGISTER_SUBMIT, &CRegisterDlg::OnBnClickedButtonRegisterSubmit)
END_MESSAGE_MAP()


// CRegisterDlg message handlers


void CRegisterDlg::OnBnClickedButtonRegisterSubmit()
{
	CString confirmPassword;
	GetDlgItemText(IDC_EDIT_REGISTER_USERNAME, m_username);
	GetDlgItemText(IDC_EDIT_REGISTER_PASSWORD, m_password);
	GetDlgItemText(IDC_EDIT_CONFIRM_PASSWORD, confirmPassword);
	m_username.Trim();
	if (m_username.IsEmpty() || m_password.IsEmpty() || confirmPassword.IsEmpty()) {
		MessageBoxW(L"Vui lòng nhập đầy đủ thông tin", L"Thông báo", MB_OK | MB_ICONERROR);
		return;
	}
	if (m_password != confirmPassword) {
		MessageBoxW(L"Vui lòng nhập đầy đủ thông tin", L"Thông báo", MB_OK | MB_ICONERROR);
		return;
	}
	EndDialog(IDOK);
}
