//
// pch.h
// Copyright (c) 2021 Marius Greuel. All rights reserved.
//

#pragma once

#define STRICT
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#define SECURITY_WIN32

#include <Windows.h>
#include <WinSock2.h>

#include <iphlpapi.h>
#include <netfw.h>
#include <shellapi.h>
#include <ShlObj.h>
#include <wincrypt.h>

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "rpcrt4.lib")

#define _ATL_NO_COM_SUPPORT
#include <atlbase.h>
#include <atlstr.h>

#define ASIO_NO_DYNAMIC_BUFFER_V1
#include <asio/error_code.hpp>

// {E757C1B5-DCE1-59AA-A641-BFA03D14E3A6}
// clang-format off
#define WPP_CONTROL_GUIDS \
    WPP_DEFINE_CONTROL_GUID(WinPX,(E757C1B5, DCE1, 59AA, A641, BFA03D14E3A6), \
        WPP_DEFINE_BIT(WppInfo) \
        WPP_DEFINE_BIT(WppWarning) \
        WPP_DEFINE_BIT(WppError) \
        WPP_DEFINE_BIT(WppTrace) \
        WPP_DEFINE_BIT(WppVerbose) \
        WPP_DEFINE_BIT(WppHighlight) \
        WPP_DEFINE_BIT(WppObject) \
        WPP_DEFINE_BIT(WppGui) \
        WPP_DEFINE_BIT(WppProxy) \
        WPP_DEFINE_BIT(WppService) \
    )
// clang-format on

#ifdef _M_IX86
#define INCLUDE_IN_PDB(x) __pragma(comment(linker, "/include:_pdb_" #x)) extern "C" x pdb_##x = {};
#else
#define INCLUDE_IN_PDB(x) __pragma(comment(linker, "/include:pdb_" #x)) extern "C" x pdb_##x = {};
#endif
