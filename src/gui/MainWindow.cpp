//
// MainWindow.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "MainWindow.h"
#include "MainWindow.tmh"

#include "DialogOptions.h"
#include "DialogWelcome.h"
#include "Firewall.h"
#include <common/ProxyCommandLineParser.h>
#include <win32/Environment.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    template<typename T>
    static inline T GetWinHttpAddress(const char* pszFunction)
    {
        auto hModule = LoadLibraryExW(L"winhttp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (hModule == nullptr)
        {
            return nullptr;
        }

        auto pFunction = ::GetProcAddress(hModule, pszFunction);
        if (pFunction == nullptr)
        {
            FreeLibrary(hModule);
            return nullptr;
        }

        return reinterpret_cast<T>(pFunction);
    }

    MainWindow::MainWindow() : m_singleInstance("MariusGreuel.WinPX")
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    MainWindow::~MainWindow()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    HWND MainWindow::Create()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_proxyConfiguration.LoadFromRegistry();

        ParseCommandLineArguments();

        if (m_options.configureFirewall)
        {
            Firewall::ConfigureFirewallForWsl(m_proxyConfiguration.port);
            SetLastError(ERROR_CANCELLED);
            return nullptr;
        }
        else if (m_options.shutdownOldInstance)
        {
            PostMessageToExistingInstance(m_nWmRequestShutdown);
            SetLastError(ERROR_CANCELLED);
            return nullptr;
        }
        else if (!m_singleInstance.IsFirst())
        {
            DoTraceMessage(WppVerbose, "Single instance only.");
            PostMessageToExistingInstance(m_nWmOtherInstance);
            SetLastError(ERROR_SINGLE_INSTANCE_APP);
            return nullptr;
        }
        else if (m_proxyConfiguration.firstRun)
        {
            DialogWelcome welcome;
            if (welcome.DoModal(m_hWnd) == S_OK)
            {
                ProxyConfiguration copy = m_proxyConfiguration;
                DialogOptions options(m_proxy, copy);
                INT_PTR result = options.DoModal(m_hWnd);
                if (result > 0)
                {
                    m_proxyConfiguration = copy;
                    m_proxyConfiguration.SaveToRegistry();
                }
                else
                {
                    DoTraceMessage(WppGui, "Operation cancelled by the user.");
                    SetLastError(ERROR_CANCELLED);
                    return nullptr;
                }
            }
            else
            {
                m_proxyConfiguration.SaveToRegistry();
            }
        }

        return CWindowImpl<MainWindow>::Create(nullptr, nullptr, nullptr, WS_OVERLAPPEDWINDOW);
    }

    int MainWindow::OnCreate(LPCREATESTRUCT /* lpCreateStruct */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        // {B09B5B31-8181-4F00-A631-CBC428ACA299}
        static const GUID guidIcon = { 0xB09B5B31, 0x8181, 0x4F00, { 0xA6, 0x31, 0xCB, 0xC4, 0x28, 0xAC, 0xA2, 0x99 } };
        if (!m_notifyIcon.Create(m_hWnd, guidIcon, m_nWmNotifyIcon))
        {
            DoTraceMessage(WppError, "Failed to create notify icon: %!WINERROR!", GetLastError());
        }

        m_notifyIcon.SetMenu(IDR_MENU_WINPX);

        UpdateUserInterface();
        RegisterProxyChangeNotification();

        if (m_proxyConfiguration.autoStartProxy)
        {
            PostMessage(WM_COMMAND, ID_PROXY_START);
        }

        if (m_options.showOptionsDialog)
        {
            PostMessage(WM_COMMAND, ID_PROXY_OPTIONS);
        }

        m_initialized = true;

        return 0;
    }

    void MainWindow::OnDestroy()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        UnregisterProxyChangeNotification();
        StopProxy();

        if (m_initialized)
        {
            m_notifyIcon.DeleteIcon();
        }

        PostQuitMessage(0);
    }

    void MainWindow::OnSettingChange(UINT uFlags, LPCTSTR lpszSection)
    {
        DoTraceMessage(WppGui, "%!FUNC!: uFlags=%u, lpszSection=%S", uFlags, lpszSection);

        UpdateUserInterface();
    }

    void MainWindow::OnSysColorChange()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        UpdateUserInterface();
    }

    void MainWindow::OnDpiChanged(UINT /* nDpiX */, UINT /* nDpiY */, PRECT /* pRect */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        UpdateUserInterface();
    }

    void MainWindow::OnContextMenu(HWND /* hWnd */, CPoint /* point */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        ShowMenu(WM_CONTEXTMENU);
    }

    LRESULT MainWindow::OnNotifyIcon(UINT /* uMsg */, WPARAM /* wParam */, LPARAM lParam, BOOL& bHandled)
    {
        DoTraceMessage(WppGui, "%!FUNC!: lParam=%llu", lParam);

        switch (lParam)
        {
        case WM_RBUTTONUP:
            ShowMenu(static_cast<UINT>(lParam));
            break;
        case WM_LBUTTONDBLCLK:
            ShowOptionsDialog();
            break;
        case NIN_BALLOONUSERCLICK:
            ShowOptionsDialog();
            break;
        default:
            bHandled = FALSE;
            break;
        }

        return 0;
    }

    LRESULT MainWindow::OnOtherInstance(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /* bHandled */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_notifyIcon.ShowInfo(_T("WinPX is already running"), _T("Click on the WinPX system tray icon to access WinPX."), NIIF_INFO);

        return 0;
    }

    LRESULT MainWindow::OnRequestShutdown(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /* bHandled */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        DestroyWindow();

        return 0;
    }

    LRESULT MainWindow::OnProxyChange(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /* bHandled */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        if (m_proxy)
        {
            m_proxy->OnSystemSettingsChange();
        }

        return 0;
    }

    void MainWindow::OnProxyStart(UINT /* uNotifyCode */, int /* nID */, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        StartProxy();
        UpdateUserInterface();
    }

    void MainWindow::OnProxyStop(UINT /* uNotifyCode */, int /* nID */, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        StopProxy();
        UpdateUserInterface();
    }

    void MainWindow::OnProxyOptions(UINT /* uNotifyCode */, int /* nID */, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        ShowOptionsDialog();
    }

    void MainWindow::OnProxyExit(UINT /* uNotifyCode */, int /* nID */, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        DestroyWindow();
    }

    void MainWindow::ShowMenu(UINT uMsg)
    {
        DoTraceMessage(WppGui, "%!FUNC!: uMsg=%u", uMsg);

        bool isRunning = m_proxy && m_proxy->GetState() == ProxyState::Running;

        m_notifyIcon.m_hMenu.GetSubMenu(0).EnableMenuItem(ID_PROXY_START, MF_BYCOMMAND | (isRunning ? MF_GRAYED : MF_ENABLED));
        m_notifyIcon.m_hMenu.GetSubMenu(0).EnableMenuItem(ID_PROXY_STOP, MF_BYCOMMAND | (isRunning ? MF_ENABLED : MF_GRAYED));

        m_notifyIcon.ShowMenu(uMsg);
    }

    void MainWindow::UpdateUserInterface()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        UpdateIcon();
        UpdateTooltip();
    }

    void MainWindow::UpdateIcon()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        bool isDarkMode = Environment::IsDarkMode();

        auto state = m_proxy ? m_proxy->GetState() : ProxyState::Unknown;
        switch (state)
        {
        case ProxyState::Running:
            m_notifyIcon.SetIcon(isDarkMode ? IDI_WINPX_DARK_RUNNING : IDI_WINPX_LIGHT_RUNNING);
            break;
        case ProxyState::Error:
            m_notifyIcon.SetIcon(isDarkMode ? IDI_WINPX_DARK_ERROR : IDI_WINPX_LIGHT_ERROR);
            break;
        case ProxyState::Stopped:
            m_notifyIcon.SetIcon(isDarkMode ? IDI_WINPX_DARK_PAUSED : IDI_WINPX_LIGHT_PAUSED);
            break;
        case ProxyState::Unknown:
        default:
            m_notifyIcon.SetIcon(isDarkMode ? IDI_WINPX_DARK_NORMAL : IDI_WINPX_LIGHT_NORMAL);
            break;
        }
    }

    void MainWindow::UpdateTooltip()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        auto state = m_proxy ? m_proxy->GetState() : ProxyState::Unknown;
        switch (state)
        {
        case ProxyState::Running:
            m_notifyIcon.SetTooltip(std::format(_T("WinPX proxy serving requests on port {}"), m_proxyConfiguration.port).c_str());
            break;
        case ProxyState::Error:
            m_notifyIcon.SetTooltip(_T("WinPX error: ") + GetLastErrorMessage());
            break;
        case ProxyState::Stopped:
        case ProxyState::Unknown:
        default:
            m_notifyIcon.SetTooltip(_T("WinPX proxy stopped"));
            break;
        }
    }

    void MainWindow::ShowOptionsDialog(UINT nStartPage)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        HWND hWndInstance = DialogOptions::GetInstance();
        if (hWndInstance != nullptr)
        {
            SetForegroundWindow(hWndInstance);
            return;
        }

        bool isRunning = m_proxy && m_proxy->GetState() == ProxyState::Running;

        ProxyConfiguration copy = m_proxyConfiguration;
        DialogOptions options(m_proxy, copy, nStartPage);
        INT_PTR result = options.DoModal(m_hWnd);
        if (result > 0)
        {
            DoTraceMessage(WppGui, "Changes were saved by the user.");

            m_proxyConfiguration = copy;
            m_proxyConfiguration.SaveToRegistry();

            if (isRunning)
            {
                StartProxy();
            }
        }
        else
        {
            DoTraceMessage(WppGui, "No changes were saved by the user.");
        }

        UpdateUserInterface();
    }

    CString MainWindow::GetLastErrorMessage()
    {
        if (!m_proxy)
        {
            return _T("No error");
        }

        auto error = m_proxy->GetErrorCode();
        auto message = error.message();
        if (error.category() == asio::system_category())
        {
            switch (error.value())
            {
            case WSAEACCES:
            case WSAEADDRINUSE:
                return std::format("The port {} is already in use by another application: {}", m_proxyConfiguration.port, message).c_str();
            }
        }

        return message.c_str();
    }

    void MainWindow::ParseCommandLineArguments()
    {
        try
        {
            ProxyCommandLineParser parser(m_proxyConfiguration);
            parser.AddOption("configure-firewall"sv, m_options.configureFirewall);
            parser.AddOption("shutdown-instance"sv, m_options.shutdownOldInstance);
            parser.AddOption("show-options"sv, m_options.showOptionsDialog);
            parser.Parse();
        }
        catch (const std::exception& e)
        {
            DoTraceMessage(WppWarning, "Failed to parse command-line: %s", e.what());
        }
    }

    void MainWindow::StartProxy()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        StopProxy();

        m_proxy = std::make_shared<Proxy>(m_proxyConfiguration);
        m_proxy->Start();

        if (m_proxy->GetState() != ProxyState::Running)
        {
            DoTraceMessage(WppError, "Failed to start proxy server.");
            m_notifyIcon.ShowInfo(_T("Failed to start proxy server"), GetLastErrorMessage(), NIIF_WARNING | NIIF_RESPECT_QUIET_TIME);
        }
    }

    void MainWindow::StopProxy()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        if (m_proxy)
        {
            m_proxy->Stop();
            m_proxy.reset();
        }
    }

    void MainWindow::RegisterProxyChangeNotification()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        auto pRegister = GetWinHttpAddress<decltype(&WinHttpRegisterProxyChangeNotification)>("WinHttpRegisterProxyChangeNotification");
        if (pRegister == nullptr)
        {
            DoTraceMessage(WppVerbose, "WinHttpRegisterProxyChangeNotification not available.");
        }
        else
        {
            ULONGLONG ullFlags = WINHTTP_PROXY_NOTIFY_CHANGE;
            DWORD dwError = pRegister(ullFlags, ProxyChangeCallback, this, &m_hProxyChangeRegistration);
            if (dwError != ERROR_SUCCESS)
            {
                DoTraceMessage(WppError, "WinHttpRegisterProxyChangeNotification failed: %!WINERROR!", dwError);
            }
        }
    }

    void MainWindow::UnregisterProxyChangeNotification()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        auto pUnregister = GetWinHttpAddress<decltype(&WinHttpUnregisterProxyChangeNotification)>("WinHttpUnregisterProxyChangeNotification");
        if (pUnregister == nullptr)
        {
            DoTraceMessage(WppVerbose, "WinHttpUnregisterProxyChangeNotification not available.");
        }
        else
        {
            if (m_hProxyChangeRegistration != nullptr)
            {
                pUnregister(m_hProxyChangeRegistration);
                m_hProxyChangeRegistration = nullptr;
            }
        }
    }

    void MainWindow::PostMessageToExistingInstance(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CWindow hWnd = FindWindow(GetWndClassInfo().m_wc.lpszClassName, nullptr);
        if (hWnd)
        {
            hWnd.PostMessage(uMsg, wParam, lParam);
        }
    }

    void MainWindow::ProxyChangeCallback(ULONGLONG ullFlags, PVOID pvContext)
    {
        DoTraceMessage(WppGui, "%!FUNC!: ullFlags=%llu", ullFlags);

        MainWindow* pThis = static_cast<MainWindow*>(pvContext);
        pThis->PostMessage(pThis->m_nWmProxyChange);
    }
}
