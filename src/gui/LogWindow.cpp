//
// LogWindow.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "LogWindow.h"
#include "LogWindow.tmh"

#include <common/Version.h>
#include <win32/Registry.h>
#include <win32/Unicode.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    struct LogWindowPosition
    {
        int showCmd;
        RECT rcNormalPosition;
    };

    LogWindow::LogWindow()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        m_hRicheditInstance = ::LoadLibraryW(CRichEditCtrl::GetLibraryName());
        ATLVERIFY(m_hRicheditInstance != nullptr);
    }

    LogWindow::~LogWindow()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        FreeLibrary(m_hRicheditInstance);
    }

    int LogWindow::OnCreate(LPCREATESTRUCT /* lpCreateStruct */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_ecMessages.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_SAVESEL);

        CIcon hIconBig;
        hIconBig.LoadIcon(IDI_WINPX, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
        SetIcon(hIconBig.Detach(), TRUE);

        CIcon hIconSmall;
        hIconSmall.LoadIcon(IDI_WINPX, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
        SetIcon(hIconSmall.Detach(), FALSE);

        CFont font;
        font.CreatePointFont(105, _T("Consolas"));
        m_ecMessages.SetFont(font.Detach());

        m_ecMessages.SendMessageW(EM_SETBKGNDCOLOR, 0, RGB(12, 12, 12));
        m_ecMessages.SendMessageW(EM_SETEVENTMASK, 0, ENM_KEYEVENTS);
        m_ecMessages.ShowScrollBar(SB_HORZ, TRUE);
        m_ecMessages.ShowScrollBar(SB_VERT, TRUE);

        LogWindowPosition position = {};
        if (SUCCEEDED(Registry::QueryValue("LogWindowPosition"sv, &position, sizeof(position))))
        {
            int width = position.rcNormalPosition.right - position.rcNormalPosition.left;
            int height = position.rcNormalPosition.bottom - position.rcNormalPosition.top;
            if (width >= GetSystemMetrics(SM_CXMIN) && width <= GetSystemMetrics(SM_CXFULLSCREEN) &&
                height >= GetSystemMetrics(SM_CYMIN) && height <= GetSystemMetrics(SM_CYFULLSCREEN))
            {
                SetWindowPos(nullptr, position.rcNormalPosition.left, position.rcNormalPosition.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
            }

            if (position.showCmd == SW_SHOWMAXIMIZED)
            {
                ShowWindowAsync(SW_SHOWMAXIMIZED);
            }
        }

        StartLogging();

        return 0;
    }

    void LogWindow::OnDestroy()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        StopLogging();

        WINDOWPLACEMENT windowPlacement = { sizeof(WINDOWPLACEMENT) };
        if (GetWindowPlacement(&windowPlacement))
        {
            LogWindowPosition position = {};
            position.showCmd = windowPlacement.showCmd;
            position.rcNormalPosition = windowPlacement.rcNormalPosition;
            Registry::SetValue("LogWindowPosition"sv, &position, sizeof(position));
        }
    }

    void LogWindow::OnSize(UINT nType, CSize size)
    {
        DoTraceMessage(WppGui, "%!FUNC!: nType=%u, size=(%d, %d)", nType, size.cx, size.cy);

        m_ecMessages.MoveWindow(0, 0, size.cx, size.cy);
    }

    void LogWindow::OnActivate(UINT nState, BOOL bMinimized, HWND /* hWndOther */)
    {
        DoTraceMessage(WppGui, "%!FUNC!: nState=%u, bMinimized=%d", nState, bMinimized);

        if (!bMinimized)
        {
            m_ecMessages.SetFocus();
        }
    }

    void LogWindow::OnContextMenu(HWND hWnd, CPoint point)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        if (hWnd == m_ecMessages.m_hWnd)
        {
            if (point.x == -1 && point.y == -1)
            {
                CRect rc;
                m_ecMessages.GetClientRect(&rc);
                point = rc.CenterPoint();
                m_ecMessages.ClientToScreen(&point);
            }

            bool hasText = m_ecMessages.GetWindowTextLength() > 0;
            bool hasSelection = m_ecMessages.GetSelectionType() != SEL_EMPTY;

            CMenu menu;
            menu.LoadMenu(IDR_MENU_LOG);
            menu.GetSubMenu(0).EnableMenuItem(ID_LOG_COPY, MF_BYCOMMAND | (hasSelection ? MF_ENABLED : MF_GRAYED));
            menu.GetSubMenu(0).EnableMenuItem(ID_LOG_SELECT_ALL, MF_BYCOMMAND | (hasText ? MF_ENABLED : MF_GRAYED));
            menu.GetSubMenu(0).EnableMenuItem(ID_LOG_CLEAR, MF_BYCOMMAND | (hasText ? MF_ENABLED : MF_GRAYED));
            menu.GetSubMenu(0).EnableMenuItem(ID_LOG_SAVEAS, MF_BYCOMMAND | (hasText ? MF_ENABLED : MF_GRAYED));
            menu.GetSubMenu(0).TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, m_hWnd);
        }
        else
        {
            SetMsgHandled(FALSE);
        }
    }

    LRESULT LogWindow::OnMsgFilter(LPNMHDR pnmh)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        if (pnmh->code == EN_MSGFILTER)
        {
            MSGFILTER* pMsgFilter = reinterpret_cast<MSGFILTER*>(pnmh);
            if (pMsgFilter->msg == WM_KEYDOWN)
            {
                bool isCtrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                if (isCtrlDown && pMsgFilter->wParam == 'X')
                {
                    OnLogClear(0, 0, nullptr);
                    return 1;
                }
                else if (isCtrlDown && pMsgFilter->wParam == 'S')
                {
                    OnLogSaveAs(0, 0, nullptr);
                    return 1;
                }
            }
        }

        return 0;
    }

    void LogWindow::OnCancel(UINT /* uNotifyCode */, int /* nID */, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        DestroyWindow();
    }

    void LogWindow::OnLogCopy(UINT /* uNotifyCode */, int /* nID */, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_ecMessages.Copy();
    }

    void LogWindow::OnLogSelectAll(UINT /* uNotifyCode */, int /* nID */, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_ecMessages.SetSel(0, -1);
    }

    void LogWindow::OnLogClear(UINT /* uNotifyCode */, int /* nID */, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_ecMessages.SetWindowText(_T(""));
    }

    void LogWindow::OnLogSaveAs(UINT /* uNotifyCode */, int /* nID */, HWND /* hWndCtl */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        if (m_ecMessages.GetWindowTextLength() == 0)
        {
            return;
        }

        LPCTSTR pszFilter = _T("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
        CFileDialog fileDialog(FALSE, _T("txt"), _T("winpx-log.txt"), OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY, pszFilter, m_hWnd);
        auto result = fileDialog.DoModal(m_hWnd);
        if (result == IDOK)
        {
            CString strFilePath = fileDialog.m_szFileName;

            std::ofstream file(strFilePath);
            if (file.is_open())
            {
                const auto timestamp = std::chrono::round<std::chrono::milliseconds>(std::chrono::system_clock::now());
                const auto localTime = std::chrono::zoned_time(std::chrono::current_zone(), timestamp).get_local_time();

                file << "\xEF\xBB\xBF";
                file << std::format("{}, log file created {}", GetProgramVersion(), localTime) << std::endl;

                CStringW strText;
                m_ecMessages.GetWindowTextW(strText);
                strText.Replace(_T("\r\n"), _T("\n"));
                file << Unicode::ToUtf8(strText.GetString());
            }
        }
    }

    void LogWindow::StartLogging()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        Logger::GetInstance().Start([this](const LogEntry& entry) { AppendLogEntry(entry); });
    }

    void LogWindow::StopLogging()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        Logger::GetInstance().Stop();
    }

    void LogWindow::AppendLogEntry(const win32::LogEntry& entry)
    {
        std::string message;

        const auto timestamp = std::chrono::round<std::chrono::milliseconds>(entry.timestamp);
        const auto localTime = std::chrono::zoned_time(std::chrono::current_zone(), timestamp).get_local_time();
        message = std::format("{} ", localTime);

        COLORREF color = RGB(204, 204, 204);

        switch (entry.level)
        {
        case LogLevel::Info:
        {
            message += "[INFO] ";
            color = RGB(204, 204, 204);
            break;
        }
        case LogLevel::Warning:
        {
            message += "[WARNING] ";
            color = RGB(249, 241, 165);
            break;
        }
        case LogLevel::Error:
        {
            message += "[ERROR] ";
            color = RGB(231, 72, 86);
            break;
        }
        }

        message += entry.message;
        message += "\n";

        AppendLogMessage(Unicode::FromUtf8(message).c_str(), color);
    }

    void LogWindow::AppendLogMessage(LPCTSTR pszMessage, COLORREF color)
    {
        int nLength = m_ecMessages.GetWindowTextLength();
        m_ecMessages.SetSel(nLength, nLength);

        CHARFORMAT2 cf = { 0 };
        cf.cbSize = sizeof(CHARFORMAT2);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = color;

        m_ecMessages.SetSelectionCharFormat(cf);
        m_ecMessages.ReplaceSel(pszMessage);

        m_ecMessages.SendMessage(WM_VSCROLL, SB_BOTTOM, 0);
    }
}
