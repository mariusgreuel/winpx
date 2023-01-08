//
// DialogWelcome.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

namespace winpx
{
    class DialogWelcome : public CTaskDialogImpl<DialogWelcome>
    {
    public:
        DialogWelcome();
        ~DialogWelcome();

    public:
        HRESULT DoModal(HWND hWndParent = nullptr);
        BOOL OnButtonClicked(int nButton);
        void OnHyperlinkClicked(LPCWSTR pszHREF);
    };
}
