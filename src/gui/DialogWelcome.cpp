//
// DialogWelcome.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "DialogWelcome.h"
#include "DialogWelcome.tmh"

#include "resource.h"
#include <win32/Unicode.h>
#include <win32/UrlLauncher.h>

namespace winpx
{
    using namespace win32;

    DialogWelcome::DialogWelcome()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    DialogWelcome::~DialogWelcome()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    HRESULT DialogWelcome::DoModal(HWND hWndParent)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        CString strContinue;
        (void)strContinue.LoadString(IDS_WELCOME_CONTINUE);

        CString strCustomize;
        (void)strCustomize.LoadString(IDS_WELCOME_CUSTOMIZE);

        const TASKDIALOG_BUTTON buttons[] = {
            { IDCANCEL, strContinue },
            { IDOK, strCustomize }
        };

        m_tdc.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_USE_COMMAND_LINKS;

        SetMainIcon(IDI_WINPX);
        SetWindowTitle(IDS_WELCOME_TITLE);
        SetMainInstructionText(IDS_WELCOME_MAIN_INSTRUCTION);
        SetContentText(IDS_WELCOME_CONTENT);
        SetButtons(buttons, ARRAYSIZE(buttons));

        int nButton = 0;
        HRESULT hr = CTaskDialogImpl<DialogWelcome>::DoModal(hWndParent, &nButton);
        if (FAILED(hr))
        {
            DoTraceMessage(WppGui, "%!FUNC! CTaskDialogImpl<DialogWelcome>::DoModal failed with hr=0x%08X", hr);
            return hr;
        }

        return nButton == IDOK ? S_OK : S_FALSE;
    }

    BOOL DialogWelcome::OnButtonClicked(int nButton)
    {
        DoTraceMessage(WppGui, "%!FUNC! nButton=%d", nButton);

        return TRUE;
    }

    void DialogWelcome::OnHyperlinkClicked(LPCWSTR pszHREF)
    {
        DoTraceMessage(WppGui, "%!FUNC! pszHREF=%S", pszHREF);

        UrlLauncher::Launch(Unicode::ToUtf8(pszHREF));
    }
}
