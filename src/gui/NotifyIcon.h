//
// NotifyIcon.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace winpx
{
    class NotifyIcon
    {
    public:
        NotifyIcon()
        {
            m_nWmTaskbarCreated = ::RegisterWindowMessage(_T("TaskbarCreated"));
        }

        BOOL Create(HWND hWnd, const GUID guid, UINT uCallbackMessage)
        {
            m_nid.hWnd = hWnd;
            m_nid.guidItem = guid;
            m_nid.uFlags = NIF_MESSAGE | NIF_GUID;
            m_nid.uCallbackMessage = uCallbackMessage;

            BOOL bSuccess = AddIcon();
            if (!bSuccess)
            {
                DeleteIcon();
                bSuccess = AddIcon();
            }

            return bSuccess;
        }

        BOOL SetIcon(ATL::_U_STRINGorID menu)
        {
            if (m_hIcon)
            {
                m_hIcon.DestroyIcon();
            }

#if _WIN32_WINNT >= _WIN32_WINNT_WIN10
            UINT dpi = GetDpiForWindow(m_nid.hWnd);
            m_hIcon.LoadIconWithScaleDown(menu, ::GetSystemMetricsForDpi(SM_CXICON, dpi), ::GetSystemMetricsForDpi(SM_CYICON, dpi));
#else
            m_hIcon.LoadIconWithScaleDown(menu, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
#endif

            m_nid.uFlags = (m_nid.uFlags & ~NIF_INFO) | NIF_ICON;
            m_nid.hIcon = m_hIcon;
            return ModifyIcon();
        }

        BOOL SetTooltip(LPCTSTR pszTooltip)
        {
            m_nid.uFlags = (m_nid.uFlags & ~NIF_INFO) | NIF_TIP;
            (void)lstrcpyn(m_nid.szTip, pszTooltip, _countof(m_nid.szTip));
            return ModifyIcon();
        }

        BOOL ShowInfo(LPCTSTR pszInfoTitle, LPCTSTR pszInfo, DWORD dwInfoFlags)
        {
            m_nid.uFlags |= NIF_INFO;
            (void)lstrcpyn(m_nid.szInfoTitle, pszInfoTitle, _countof(m_nid.szInfoTitle));
            (void)lstrcpyn(m_nid.szInfo, pszInfo, _countof(m_nid.szInfo));
            m_nid.dwInfoFlags = dwInfoFlags;
            return ModifyIcon();
        }

        void SetMenu(ATL::_U_STRINGorID menu)
        {
            m_hMenu.LoadMenu(menu);
        }

        BOOL AddIcon()
        {
            return Shell_NotifyIcon(NIM_ADD, &m_nid);
        }

        BOOL ModifyIcon()
        {
            return Shell_NotifyIcon(NIM_MODIFY, &m_nid);
        }

        BOOL DeleteIcon()
        {
            return Shell_NotifyIcon(NIM_DELETE, &m_nid);
        }

        BEGIN_MSG_MAP(CMyWindow)
            MESSAGE_HANDLER(m_nWmTaskbarCreated, OnTaskbarCreated)
            MESSAGE_HANDLER(m_nid.uCallbackMessage, OnCallbackMessage)
        END_MSG_MAP()

        LRESULT OnTaskbarCreated(UINT /* uMsg */, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /* bHandled */)
        {
            AddIcon();
            return 0;
        }

        LRESULT OnCallbackMessage(UINT /* uMsg */, WPARAM /* wParam */, LPARAM lParam, BOOL& bHandled)
        {
            switch (lParam)
            {
            case WM_RBUTTONUP:
                if (!ShowMenu(static_cast<UINT>(lParam)))
                {
                    bHandled = FALSE;
                }
                return 0;
            default:
                bHandled = FALSE;
                return 0;
            }
        }

        BOOL ShowMenu(UINT uMsg)
        {
            if (!m_hMenu.IsMenu())
            {
                return FALSE;
            }

            SetForegroundWindow(m_nid.hWnd);

            UINT nFlags = uMsg == WM_RBUTTONUP ? TPM_RIGHTBUTTON : TPM_LEFTBUTTON;

            CPoint pt;
            ::GetCursorPos(&pt);
            m_hMenu.GetSubMenu(0).TrackPopupMenu(nFlags, pt.x, pt.y, m_nid.hWnd, nullptr);

            return TRUE;
        }

    public:
        NOTIFYICONDATA m_nid = { sizeof(NOTIFYICONDATA) };
        UINT m_nWmTaskbarCreated = 0;
        CIcon m_hIcon;
        CMenu m_hMenu;
    };
}
