//
// MainWindow.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "NotifyIcon.h"
#include "Options.h"
#include "resource.h"
#include <common/Proxy.h>
#include <common/ProxyConfiguration.h>
#include <win32/SingleInstance.h>

namespace winpx
{
    class MainWindow : public CWindowImpl<MainWindow>
    {
    public:
        MainWindow();
        ~MainWindow();

        MainWindow(const MainWindow&) = delete;
        MainWindow& operator=(const MainWindow&) = delete;

    public:
        HWND Create();

        DECLARE_WND_CLASS(_T("WinPX"))

        BEGIN_MSG_MAP(CMyWindow)
            MSG_WM_CREATE(OnCreate)
            MSG_WM_DESTROY(OnDestroy)
            MSG_WM_SETTINGCHANGE(OnSettingChange)
            MSG_WM_SYSCOLORCHANGE(OnSysColorChange)
            MSG_WM_DPICHANGED(OnDpiChanged)
            MSG_WM_CONTEXTMENU(OnContextMenu)
            MESSAGE_HANDLER(m_nWmNotifyIcon, OnNotifyIcon)
            MESSAGE_HANDLER(m_nWmOtherInstance, OnOtherInstance)
            MESSAGE_HANDLER(m_nWmRequestShutdown, OnRequestShutdown)
            MESSAGE_HANDLER(m_nWmProxyChange, OnProxyChange)
            COMMAND_ID_HANDLER_EX(ID_PROXY_START, OnProxyStart)
            COMMAND_ID_HANDLER_EX(ID_PROXY_STOP, OnProxyStop)
            COMMAND_ID_HANDLER_EX(ID_PROXY_OPTIONS, OnProxyOptions)
            COMMAND_ID_HANDLER_EX(ID_PROXY_EXIT, OnProxyExit)
            CHAIN_MSG_MAP_MEMBER(m_notifyIcon)
        END_MSG_MAP()

    private:
        int OnCreate(LPCREATESTRUCT lpCreateStruct);
        void OnDestroy();
        void OnSettingChange(UINT uFlags, LPCTSTR lpszSection);
        void OnSysColorChange();
        void OnDpiChanged(UINT nDpiX, UINT nDpiY, PRECT pRect);
        void OnContextMenu(HWND hWnd, CPoint point);
        LRESULT OnNotifyIcon(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
        LRESULT OnOtherInstance(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
        LRESULT OnRequestShutdown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
        LRESULT OnProxyChange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
        void OnProxyStart(UINT uNotifyCode, int nID, HWND hWndCtl);
        void OnProxyStop(UINT uNotifyCode, int nID, HWND hWndCtl);
        void OnProxyOptions(UINT uNotifyCode, int nID, HWND hWndCtl);
        void OnProxyExit(UINT uNotifyCode, int nID, HWND hWndCtl);

        void ShowMenu(UINT uMsg);
        void UpdateUserInterface();
        void UpdateIcon();
        void UpdateTooltip();
        void ShowOptionsDialog(UINT nStartPage = 0);

        CString GetLastErrorMessage();

        void ParseCommandLineArguments();
        void StartProxy();
        void StopProxy();
        void RegisterProxyChangeNotification();
        void UnregisterProxyChangeNotification();
        void PostMessageToExistingInstance(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0);
        static void WINAPI ProxyChangeCallback(ULONGLONG ullFlags, PVOID pvContext);

    private:
        static const UINT m_nWmNotifyIcon = WM_APP;
        static const UINT m_nWmOtherInstance = WM_APP + 1;
        static const UINT m_nWmRequestShutdown = WM_APP + 2;
        static const UINT m_nWmProxyChange = WM_APP + 3;
        win32::SingleInstance m_singleInstance;
        NotifyIcon m_notifyIcon;
        Options m_options;
        ProxyConfiguration m_proxyConfiguration;
        std::shared_ptr<Proxy> m_proxy;
        bool m_initialized = false;
        WINHTTP_PROXY_CHANGE_REGISTRATION_HANDLE m_hProxyChangeRegistration = nullptr;
    };
}
