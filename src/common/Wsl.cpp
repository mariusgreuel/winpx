//
// Wsl.cpp
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#include <pch.h>

#include "Wsl.h"
#include "Wsl.tmh"

#include "Tools.h"
#include <win32/Win32.h>

namespace winpx
{
    using namespace std::literals::string_view_literals;
    using namespace win32;

    bool Wsl::IsVersion2Used()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        CRegKey keyLxss;
        LONG nError = keyLxss.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Lxss"), KEY_READ);
        if (nError != ERROR_SUCCESS)
        {
            return false;
        }

        for (DWORD dwIndex = 0;; dwIndex++)
        {
            DWORD dwSize = 256;
            std::vector<TCHAR> buffer(dwSize);
            nError = keyLxss.EnumKey(dwIndex, buffer.data(), &dwSize);
            if (nError == ERROR_MORE_DATA)
            {
                buffer.resize(static_cast<size_t>(dwSize) + 1);
                dwSize = static_cast<DWORD>(buffer.size());
                nError = keyLxss.EnumKey(dwIndex, buffer.data(), &dwSize);
            }

            if (nError == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            else if (nError == ERROR_SUCCESS)
            {
                CRegKey keyDistro;
                nError = keyDistro.Open(keyLxss, buffer.data(), KEY_READ);
                if (nError == ERROR_SUCCESS)
                {
                    DWORD dwVersion = 0;
                    keyDistro.QueryDWORDValue(_T("Version"), dwVersion);
                    if (dwVersion >= 2)
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool Wsl::UsesNatNetworkingMode()
    {
        return IEquals(GetNetworkingMode(), "Nat"sv);
    }

    std::string Wsl::GetNetworkingMode()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        auto path = Win32::GetKnownFolderPath(FOLDERID_Profile) / ".wslconfig";

        std::error_code error;
        if (std::filesystem::exists(path, error))
        {
            std::ifstream stream(path);
            if (stream)
            {
                return ParseWslNetworkingMode(stream);
            }
        }

        return "Nat";
    }

    std::string Wsl::ParseWslNetworkingMode(std::istream& stream)
    {
        bool inWsl2Section = false;

        std::string line;
        while (std::getline(stream, line))
        {
            auto view = Trim(std::string_view(line), " \t");
            if (view.empty() || view.starts_with(';') || view.starts_with('#'))
                continue;

            if (view.front() == '[' && view.back() == ']')
            {
                auto section = Trim(view.substr(1, view.size() - 2), " \t");
                inWsl2Section = IEquals(section, "wsl2"sv) || IEquals(section, "experimental"sv);
                continue;
            }

            if (inWsl2Section)
            {
                auto equal = view.find('=');
                if (equal != std::string::npos)
                {
                    auto key = Trim(view.substr(0, equal), " \t");
                    auto value = Trim(view.substr(equal + 1), " \t");
                    if (IEquals(key, "networkingMode"sv))
                    {
                        return std::string(value);
                    }
                }
            }
        }

        return "Nat";
    }

    std::string Wsl::GetVirtualSwitchAddress()
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        auto address = FindAdapterByFriendlyName(L"WSL"sv);
        if (!address.empty())
        {
            DoTraceMessage(WppVerbose, "Found WSL virtual switch: %!str!", address);
            return address;
        }

        return {};
    }

    std::string Wsl::FindAdapterByFriendlyName(std::wstring_view name)
    {
        DoTraceMessage(WppVerbose, "%!FUNC!");

        ULONG nBufferSize = 15 * 1024;

        std::vector<std::byte> buffer(nBufferSize);
        auto pAdapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

        ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_DNS_INFO;
        ULONG status = GetAdaptersAddresses(AF_INET, flags, nullptr, pAdapters, &nBufferSize);
        if (status == ERROR_BUFFER_OVERFLOW)
        {
            buffer.resize(nBufferSize);
            pAdapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
            status = GetAdaptersAddresses(AF_INET, flags, nullptr, pAdapters, &nBufferSize);
        }

        if (status != ERROR_SUCCESS)
        {
            DoTraceMessage(WppWarning, "GetAdaptersAddresses failed: %!WINERROR!", status);
        }
        else
        {
            for (auto pAdapter = pAdapters; pAdapter != nullptr; pAdapter = pAdapter->Next)
            {
                if (pAdapter->FriendlyName != nullptr)
                {
                    DoTraceMessage(WppVerbose, "Checking adapter: GUID='%s', Description='%S', FriendlyName='%S'", pAdapter->AdapterName, pAdapter->Description, pAdapter->FriendlyName);

                    std::wstring friendlyName(pAdapter->FriendlyName);
                    if (friendlyName.find(name) != std::wstring::npos)
                    {
                        return GetFirstIPv4Address(pAdapter);
                    }
                }
            }
        }

        return {};
    }

    std::string Wsl::GetFirstIPv4Address(PIP_ADAPTER_ADDRESSES pAdapter)
    {
        for (auto pUnicast = pAdapter->FirstUnicastAddress; pUnicast != nullptr; pUnicast = pUnicast->Next)
        {
            if (pUnicast->Address.lpSockaddr->sa_family == AF_INET)
            {
                sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(pUnicast->Address.lpSockaddr);

                char buffer[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &(sa_in->sin_addr), buffer, INET_ADDRSTRLEN) != nullptr)
                {
                    return buffer;
                }
            }
        }

        return {};
    }
}
