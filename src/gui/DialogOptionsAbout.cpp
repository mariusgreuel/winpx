//
// DialogOptionsAbout.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include "pch.h"

#include "DialogOptionsAbout.h"
#include "DialogOptionsAbout.tmh"

#include "OnlineHelp.h"
#include <common/Resources.h>
#include <common/Version.h>
#include <win32/Unicode.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    DialogOptionsAbout::DialogOptionsAbout()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        EnableHelp();
    }

    DialogOptionsAbout::~DialogOptionsAbout()
    {
        DoTraceMessage(WppObject, "%!FUNC!");
    }

    LRESULT DialogOptionsAbout::OnInitDialog(HWND /* hWndCtl */, LPARAM /* lParam */)
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        m_stVersion.Attach(GetDlgItem(IDC_ABOUT_VERSION));
        m_stLink.SubclassWindow(GetDlgItem(IDC_ABOUT_LINK));
        m_stLicenses.Attach(GetDlgItem(IDC_ABOUT_LICENSES));

        CString strGitCommitHash = _T(GIT_COMMIT_HASH);

        CString strVersion;
        m_stVersion.GetWindowText(strVersion);
        strVersion.Format(strVersion, VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
        if (!strGitCommitHash.IsEmpty())
            strVersion += L" (" + strGitCommitHash.Left(8) + L")";
        m_stVersion.SetWindowText(strVersion);

        m_stLicenses.SetWindowText(Unicode::FromUtf8(Resources::GetLicenses()).c_str());

        return 0;
    }

    void DialogOptionsAbout::OnHelp()
    {
        DoTraceMessage(WppGui, "%!FUNC!");

        OnlineHelp::Open("About"sv);
    }
}
