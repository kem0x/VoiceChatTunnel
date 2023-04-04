#pragma once
#include <fstream>
#include <vector>
#include <strsafe.h>
#include <icmpapi.h>
#include <Dbghelp.h>
#include <Shlobj.h>
#include <Iphlpapi.h>

#define EXECUTE_ONE_TIME if (([] { 		    \
    static bool is_first_time = true;	    \
    auto was_first_time = is_first_time;    \
    is_first_time = false; 					\
    return was_first_time; }()))

namespace util
{
    static auto ping(const char* host, int timeout = 1000)
    {
        long p = 999;

        wchar_t buf[MAX_PATH + 1] = {};
        HANDLE hIcmpFile = IcmpCreateFile();
        DWORD ip = inet_addr(host);
        DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(buf);

        void* replayBuffer = static_cast<void*>(malloc(replySize));

        if (hIcmpFile != INVALID_HANDLE_VALUE || replayBuffer)
        {
            IcmpSendEcho(hIcmpFile, ip, buf, sizeof(buf), nullptr, replayBuffer, replySize, timeout);

            auto echoReply = static_cast<PICMP_ECHO_REPLY>(replayBuffer);
            if (echoReply->Status == IP_SUCCESS)
            {
                p = echoReply->RoundTripTime;
            }

            free(replayBuffer);
        }

        return p;
    }

    std::string readFile(const std::string& path)
    {
        std::ifstream input_file(path);
        if (!input_file.is_open())
        {
            return "";
        }

        return std::string((std::istreambuf_iterator<char>(input_file)), std::istreambuf_iterator<char>());
    }

    std::vector<std::string> split(std::string& str, std::string sep)
    {
        char* cstr = const_cast<char*>(str.c_str());
        char* current;
        std::vector<std::string> arr;
        current = strtok(cstr, sep.c_str());
        while (current != NULL)
        {
            arr.push_back(current);
            current = strtok(NULL, sep.c_str());
        }
        return arr;
    }

    constexpr unsigned int str2int(const char* str, int h = 0)
    {
        return !str[h] ? 5381 : (str2int(str, h + 1) * 33) ^ str[h];
    }

    void make_minidump(EXCEPTION_POINTERS* e)
    {
        TCHAR tszFileName[1024] = { 0 };
        TCHAR tszPath[1024] = { 0 };
        SYSTEMTIME stTime = { 0 };
        GetSystemTime(&stTime);
        SHGetSpecialFolderPath(NULL, tszPath, CSIDL_APPDATA, FALSE);
        StringCbPrintf(tszFileName, _countof(tszFileName), L"%s\\%s__%4d%02d%02d_%02d%02d%02d.dmp", tszPath, L"CrashDump", stTime.wYear, stTime.wMonth, stTime.wDay, stTime.wHour, stTime.wMinute, stTime.wSecond);

        HANDLE hFile = CreateFile(tszFileName, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if (hFile == INVALID_HANDLE_VALUE)
            return;

        MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
        exceptionInfo.ThreadId = GetCurrentThreadId();
        exceptionInfo.ExceptionPointers = e;
        exceptionInfo.ClientPointers = FALSE;

        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory | MiniDumpWithFullMemory),
            e ? &exceptionInfo : NULL,
            NULL,
            NULL);

        if (hFile)
        {
            CloseHandle(hFile);
            hFile = NULL;
        }
        return;
    }

    bool isAutoStartUpAlready()
    {
        HKEY hKey = NULL;
        LONG lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey);
        if (lResult != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD dwType = REG_SZ;
        DWORD dwSize = 0;
        lResult = RegQueryValueEx(hKey, L"VCT", NULL, &dwType, NULL, &dwSize);
        if (lResult != ERROR_SUCCESS)
        {
            return false;
        }

        wchar_t* pValue = new wchar_t[dwSize];
        lResult = RegQueryValueEx(hKey, L"VCT", NULL, &dwType, (LPBYTE)pValue, &dwSize);
        if (lResult != ERROR_SUCCESS)
        {
            return false;
        }

        delete[] pValue;
        pValue = NULL;

        RegCloseKey(hKey);
        hKey = NULL;

        return true;
    }

    void createOrDeleteAutoStartUp()
    {
        HKEY hKey = NULL;
        LONG lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hKey);
        if (lResult != ERROR_SUCCESS)
        {
            return;
        }

        wchar_t szPath[MAX_PATH] = { 0 };
        GetModuleFileNameW(NULL, szPath, MAX_PATH);
        auto strPath = L'"' + std::wstring(szPath) + L'"';

        if (isAutoStartUpAlready())
        {
            lResult = RegDeleteValue(hKey, L"VCT");
            if (lResult != ERROR_SUCCESS)
            {
                return;
            }
        }
        else
        {
            lResult = RegSetValueEx(hKey, L"VCT", 0, REG_SZ, (LPBYTE)strPath.c_str(), (strPath.length() + 1) * sizeof(wchar_t));
            if (lResult != ERROR_SUCCESS)
            {
                return;
            }
        }

        RegCloseKey(hKey);
        hKey = NULL;
    }
}