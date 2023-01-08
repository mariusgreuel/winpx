//
// DialogOptionsAbout.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once
#include "resource.h"

namespace winpx
{
    class DialogOptionsAbout : public CPropertyPageImpl<DialogOptionsAbout>
    {
    public:
        DialogOptionsAbout();
        ~DialogOptionsAbout();

    public:
        static constexpr auto IDD = IDD_OPTIONS_ABOUT;

        BEGIN_MSG_MAP(DialogOptionsAbout)
            MSG_WM_INITDIALOG(OnInitDialog)
            CHAIN_MSG_MAP(CPropertyPageImpl<DialogOptionsAbout>)
        END_MSG_MAP()

        LRESULT OnInitDialog(HWND hWndCtl, LPARAM lParam);
        void OnHelp();

    private:
        CStatic m_stVersion;
        CHyperLink m_stLink;
        CStatic m_stLicenses;
    };
}
