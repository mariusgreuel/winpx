//
// MitigationPolicy.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "MitigationPolicy.h"
#include "MitigationPolicy.tmh"

namespace win32
{
    static void SetProcessDynamicCodePolicy()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy = {};
        policy.ProhibitDynamicCode = 1;
        if (!SetProcessMitigationPolicy(ProcessDynamicCodePolicy, &policy, sizeof(policy)))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppWarning, "Failed to set dynamic code policy: %!WINERROR!", dwError);
        }
    }

    static void SetProcessFontDisablePolicy()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        PROCESS_MITIGATION_FONT_DISABLE_POLICY policy = {};
        policy.DisableNonSystemFonts = 1;
        if (!SetProcessMitigationPolicy(ProcessFontDisablePolicy, &policy, sizeof(policy)))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppWarning, "Failed to set font disable policy: %!WINERROR!", dwError);
        }
    }

    static void SetProcessImageLoadPolicy()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        PROCESS_MITIGATION_IMAGE_LOAD_POLICY policy = {};
        policy.NoRemoteImages = 1;
        policy.NoLowMandatoryLabelImages = 1;
        policy.PreferSystem32Images = 1;
        if (!SetProcessMitigationPolicy(ProcessImageLoadPolicy, &policy, sizeof(policy)))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppWarning, "Failed to set image load policy: %!WINERROR!", dwError);
        }
    }

    static void SetProcessSignaturePolicy()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY policy = {};
        policy.MicrosoftSignedOnly = 1;
        if (!SetProcessMitigationPolicy(ProcessSignaturePolicy, &policy, sizeof(policy)))
        {
            DWORD dwError = GetLastError();
            DoTraceMessage(WppWarning, "Failed to set signature policy: %!WINERROR!", dwError);
        }
    }

    void MitigationPolicy::ApplyToProcess()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        static std::once_flag once;
        std::call_once(once, [] {
            SetProcessDynamicCodePolicy();
            SetProcessFontDisablePolicy();
            SetProcessImageLoadPolicy();
            SetProcessSignaturePolicy();
        });
    }
}
