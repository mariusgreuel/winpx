//
// WindowsFirewall.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "WindowsFirewall.h"
#include "WindowsFirewall.tmh"

#include "Unicode.h"
#include "Win32Error.h"
#include <comdef.h>

namespace win32
{
    _COM_SMARTPTR_TYPEDEF(INetFwPolicy2, __uuidof(INetFwPolicy2));
    _COM_SMARTPTR_TYPEDEF(INetFwRules, __uuidof(INetFwRules));
    _COM_SMARTPTR_TYPEDEF(INetFwRule, __uuidof(INetFwRule));

    WindowsFirewall::WindowsFirewall()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        m_hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(m_hrCom))
        {
            DoTraceMessage(WppError, "CoInitializeEx failed: %!HRESULT!", m_hrCom);
            throw Win32Error(m_hrCom, "Failed to initialize COM library");
        }
    }

    WindowsFirewall::~WindowsFirewall()
    {
        DoTraceMessage(WppObject, "%!FUNC!");

        if (SUCCEEDED(m_hrCom))
        {
            CoUninitialize();
        }
    }

    void WindowsFirewall::AddRule(const FirewallRule& rule)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        INetFwPolicy2Ptr pFwPolicy;
        HRESULT hr = pFwPolicy.CreateInstance(__uuidof(NetFwPolicy2));
        if (FAILED(hr))
        {
            DoTraceMessage(WppError, "Failed to create instance of NetFwPolicy2: %!HRESULT!", hr);
            throw Win32Error(hr, "Failed to create instance of NetFwPolicy2");
        }

        INetFwRulesPtr pFwRules;
        hr = pFwPolicy->get_Rules(&pFwRules);
        if (FAILED(hr))
        {
            DoTraceMessage(WppError, "Failed to get rules from NetFwPolicy2: %!HRESULT!", hr);
            throw Win32Error(hr, "Failed to get rules from NetFwPolicy2");
        }

        INetFwRulePtr pFwRule;
        hr = pFwRule.CreateInstance(__uuidof(NetFwRule));
        if (FAILED(hr))
        {
            DoTraceMessage(WppError, "Failed to create instance of NetFwRule: %!HRESULT!", hr);
            throw Win32Error(hr, "Failed to create instance of NetFwRule");
        }

        pFwRule->put_Name(_bstr_t(Unicode::FromUtf8(rule.name).c_str()));
        pFwRule->put_Description(_bstr_t(Unicode::FromUtf8(rule.description).c_str()));
        pFwRule->put_Grouping(_bstr_t(Unicode::FromUtf8(rule.group).c_str()));
        pFwRule->put_ApplicationName(_bstr_t(rule.applicationPath.native().c_str()));
        pFwRule->put_Protocol(rule.protocol);
        pFwRule->put_Direction(rule.direction);
        pFwRule->put_Action(rule.action);
        pFwRule->put_Profiles(rule.profiles);

        if (!rule.localAddress.empty())
        {
            pFwRule->put_LocalAddresses(_bstr_t(Unicode::FromUtf8(rule.localAddress).c_str()));
        }

        if (!rule.remoteAddress.empty())
        {
            pFwRule->put_RemoteAddresses(_bstr_t(Unicode::FromUtf8(rule.remoteAddress).c_str()));
        }

        if (!rule.localPorts.empty())
        {
            pFwRule->put_LocalPorts(_bstr_t(Unicode::FromUtf8(rule.localPorts).c_str()));
        }

        if (!rule.remotePorts.empty())
        {
            pFwRule->put_RemotePorts(_bstr_t(Unicode::FromUtf8(rule.remotePorts).c_str()));
        }

        pFwRule->put_Enabled(rule.enabled ? VARIANT_TRUE : VARIANT_FALSE);

        hr = pFwRules->Add(pFwRule);
        if (FAILED(hr))
        {
            DoTraceMessage(WppError, "Failed to add rule to NetFwRules: %!HRESULT!", hr);
            throw Win32Error(hr, "Failed to add rule to NetFwRules");
        }
    }

    void WindowsFirewall::RemoveAllRules(std::string_view name)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        INetFwPolicy2Ptr pFwPolicy;
        HRESULT hr = pFwPolicy.CreateInstance(__uuidof(NetFwPolicy2));
        if (FAILED(hr))
        {
            DoTraceMessage(WppError, "Failed to create instance of NetFwPolicy2: %!HRESULT!", hr);
            throw Win32Error(hr, "Failed to create instance of NetFwPolicy2");
        }

        INetFwRulesPtr pFwRules;
        hr = pFwPolicy->get_Rules(&pFwRules);
        if (FAILED(hr))
        {
            DoTraceMessage(WppError, "Failed to get rules from NetFwPolicy2: %!HRESULT!", hr);
            throw Win32Error(hr, "Failed to get rules from NetFwPolicy2");
        }

        _bstr_t bstrName(Unicode::FromUtf8(name).c_str());
        for (int i = 0; i < 4; i++)
        {
            hr = pFwRules->Remove(bstrName);
            if (FAILED(hr))
            {
                break;
            }
        }
    }
}
