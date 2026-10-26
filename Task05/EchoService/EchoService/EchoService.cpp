#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <thread>
#include <mutex>

#pragma comment(lib, "Ws2_32.lib")

static const wchar_t* SERVICE_NAME = L"ChatEchoService";
static const wchar_t* SERVICE_DISPLAY_NAME = L"Chat Echo Service";
static const int LISTEN_PORT = 9999;

static SERVICE_STATUS        g_ServiceStatus = { 0 };
static SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
static HANDLE                g_hStopEvent = NULL;

static std::vector<std::thread> g_clientThreads;
static std::mutex               g_threadsMutex;

static bool IsStopping()
{
    return WaitForSingleObject(g_hStopEvent, 0) == WAIT_OBJECT_0;
}

static int WaitReadable(SOCKET s)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(s, &readfds);

    timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    return select(0, &readfds, NULL, NULL, &tv);
}

static void HandleClient(SOCKET clientSocket)
{
    char buffer[4096];

    while (!IsStopping())
    {
        int sel = WaitReadable(clientSocket);
        if (sel == SOCKET_ERROR) break;
        if (sel == 0) continue;

        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) break;

        if (send(clientSocket, buffer, bytesReceived, 0) == SOCKET_ERROR) break;
    }

    closesocket(clientSocket);
}

static void ServiceWorker()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    sockaddr_in service = {};
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(LISTEN_PORT);

    if (bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR ||
        listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    while (!IsStopping())
    {
        int sel = WaitReadable(listenSocket);
        if (sel == SOCKET_ERROR) break;
        if (sel == 0) continue;

        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) continue;

        std::lock_guard<std::mutex> lock(g_threadsMutex);
        g_clientThreads.emplace_back(HandleClient, clientSocket);
    }

    closesocket(listenSocket);

    {
        std::lock_guard<std::mutex> lock(g_threadsMutex);
        for (auto& t : g_clientThreads) {
            if (t.joinable()) t.join();
        }
        g_clientThreads.clear();
    }

    WSACleanup();
}

static void ReportStatus(DWORD state, DWORD exitCode, DWORD waitHint)
{
    static DWORD checkpoint = 1;
    g_ServiceStatus.dwCurrentState = state;
    g_ServiceStatus.dwWin32ExitCode = exitCode;
    g_ServiceStatus.dwWaitHint = waitHint;

    g_ServiceStatus.dwControlsAccepted =
        (state == SERVICE_START_PENDING) ? 0 : SERVICE_ACCEPT_STOP;

    g_ServiceStatus.dwCheckPoint =
        (state == SERVICE_RUNNING || state == SERVICE_STOPPED) ? 0 : checkpoint++;

    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

static void WINAPI ServiceCtrlHandler(DWORD ctrlCode)
{
    if (ctrlCode == SERVICE_CONTROL_STOP && g_ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
        ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 4000);
        SetEvent(g_hStopEvent);
    }
}

static void WINAPI SvcMain(DWORD, LPWSTR*)
{
    g_StatusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_StatusHandle) return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ReportStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    g_hStopEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!g_hStopEvent) {
        ReportStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    ReportStatus(SERVICE_RUNNING, NO_ERROR, 0);

    ServiceWorker();

    CloseHandle(g_hStopEvent);
    ReportStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

static void InstallService()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);

    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        printf("OpenSCManager failed (%lu)\n", GetLastError());
        return;
    }

    SC_HANDLE svc = CreateServiceW(
        scm, SERVICE_NAME, SERVICE_DISPLAY_NAME,
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        path, NULL, NULL, NULL, NULL, NULL);

    if (svc) {
        printf("Service installed successfully\n");
        CloseServiceHandle(svc);
    }
    else {
        printf("CreateService failed (%lu)\n", GetLastError());
    }
    CloseServiceHandle(scm);
}

static void UninstallService()
{
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        printf("OpenSCManager failed (%lu)\n", GetLastError());
        return;
    }

    SC_HANDLE svc = OpenServiceW(scm, SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (svc) {
        SERVICE_STATUS status;
        ControlService(svc, SERVICE_CONTROL_STOP, &status);
        if (DeleteService(svc))
            printf("Service deleted successfully\n");
        else
            printf("DeleteService failed (%lu)\n", GetLastError());
        CloseServiceHandle(svc);
    }
    else {
        printf("OpenService failed (%lu)\n", GetLastError());
    }
    CloseServiceHandle(scm);
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc > 1 && _wcsicmp(argv[1], L"install") == 0) {
        InstallService();
        return 0;
    }
    if (argc > 1 && _wcsicmp(argv[1], L"uninstall") == 0) {
        UninstallService();
        return 0;
    }

    SERVICE_TABLE_ENTRYW serviceTable[] = {
        { (LPWSTR)SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)SvcMain },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcherW(serviceTable))
        printf("Run as a service, or use: EchoService.exe install | uninstall\n");
    return 0;
}
