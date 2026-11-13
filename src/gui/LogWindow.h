//
// LogWindow.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "resource.h"

#include <win32/Logger.h>

namespace winpx
{
    class LogWindow : public CWindowImpl<LogWindow>
    {
    public:
        LogWindow();
        ~LogWindow();

        LogWindow(const LogWindow&) = delete;
        LogWindow& operator=(const LogWindow&) = delete;

    public:
        DECLARE_WND_CLASS(_T("WinPX-Log"))

        BEGIN_MSG_MAP(LogWindow)
            MSG_WM_CREATE(OnCreate)
            MSG_WM_DESTROY(OnDestroy)
            MSG_WM_SIZE(OnSize)
            MSG_WM_ACTIVATE(OnActivate)
            MSG_WM_CONTEXTMENU(OnContextMenu)
            NOTIFY_CODE_HANDLER_EX(EN_MSGFILTER, OnMsgFilter)
            COMMAND_ID_HANDLER_EX(IDCANCEL, OnCancel)
            COMMAND_ID_HANDLER_EX(ID_LOG_COPY, OnLogCopy)
            COMMAND_ID_HANDLER_EX(ID_LOG_SELECT_ALL, OnLogSelectAll)
            COMMAND_ID_HANDLER_EX(ID_LOG_CLEAR, OnLogClear)
            COMMAND_ID_HANDLER_EX(ID_LOG_SAVEAS, OnLogSaveAs)
        END_MSG_MAP()

        int OnCreate(LPCREATESTRUCT lpCreateStruct);
        void OnDestroy();
        void OnSize(UINT nType, CSize size);
        void OnActivate(UINT nState, BOOL bMinimized, HWND hWndOther);
        void OnContextMenu(HWND hWnd, CPoint point);
        LRESULT OnMsgFilter(LPNMHDR pnmh);
        void OnCancel(UINT uNotifyCode, int nID, HWND hWndCtl);
        void OnLogCopy(UINT uNotifyCode, int nID, HWND hWndCtl);
        void OnLogSelectAll(UINT uNotifyCode, int nID, HWND hWndCtl);
        void OnLogClear(UINT uNotifyCode, int nID, HWND hWndCtl);
        void OnLogSaveAs(UINT uNotifyCode, int nID, HWND hWndCtl);

    private:
        void StartLogging();
        void StopLogging();
        void AppendLogEntry(const win32::LogEntry& entry);
        void AppendLogMessage(LPCTSTR pszMessage, COLORREF color);

    private:
        HMODULE m_hRicheditInstance = nullptr;
        CRichEditCtrl m_ecMessages;
    };
}
