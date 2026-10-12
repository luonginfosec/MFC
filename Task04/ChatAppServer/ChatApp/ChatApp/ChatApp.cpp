#include "pch.h"
#include "ChatApp.h"
#include "ChatAppDlg.h"

BEGIN_MESSAGE_MAP(CChatAppApp, CWinApp)
END_MESSAGE_MAP()

CChatAppApp theApp;

CChatAppApp::CChatAppApp()
{
}

BOOL CChatAppApp::InitInstance()
{
    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&controls);
    CWinApp::InitInstance();
    AfxEnableControlContainer();
    CChatAppDlg dialog;
    m_pMainWnd = &dialog;
    dialog.DoModal();
    return FALSE;
}
