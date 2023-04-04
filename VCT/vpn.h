#pragma once
#define CPPHTTPLIB_OPENSSL_SUPPORT

#include "include/httplib.h"
#include "include/inicpp.h"
#include "include/csv.h"

/*
 KNOWN ISSUE(S) :
    - If the adapter was connected before this was launched, it doesn't disconnect even it returns true
*/

constexpr const char* VPN_CONNECTION_NAME = "VCT";
const std::vector<std::pair<std::string, std::string>> VOIP_ROUTES = {
    { "188.42.95.0", "255.255.255.0" },
    { "37.244.0.0", "255.255.0.0" },
};

class VPN
{
    HRASCONN connection = nullptr;
    PIP_ADAPTER_INFO _interface = nullptr;

public:
    std::pair<std::string, std::string> currentServer = { "", "" };
    std::vector<std::pair<std::string, std::string>> serversList = {};

    void parseServers(const std::string& data)
    {
        std::stringstream ss(data);
        std::istream is(ss.flush().rdbuf());

        io::CSVReader<7> in("servers", is);
        in.read_header(io::ignore_extra_column, "HostName", "IP", "Score", "Ping", "Speed", "CountryLong", "CountryShort");

        std::string HostName;
        std::string IP;
        long long int Score;
        int Ping;
        long long int Speed;
        std::string CountryLong;
        std::string CountryShort;

        while (in.read_row(HostName, IP, Score, Ping, Speed, CountryLong, CountryShort))
        {
            // printf("IP: %s - Round network trip: %i\n", IP.c_str(), Ping);
            this->serversList.push_back({ IP, (HostName + ".opengw.net") });
        }
    }

    void getServersList()
    {
        httplib::Client cli("https://www.vpngate.net");

        auto r = cli.Get("/api/iphone/");

        if (r->status == 200)
        {
            // the csv here is not valid, but it's the only way to get the data so we fix it
            r->body.erase(0, 15); // removes (*vpn_servers\n#)
            r->body.resize(r->body.size() - 3); // removes (*)

            return parseServers(r->body);
        }
        else
        {
            printf("Status: %i\n", r->status);
        }

        auto ret = MessageBoxA(nullptr, "An error occured while getting servers list.\nPlease make sure you have a stable internet connection and press retry or press cancel to exit.", "Caution!", MB_RETRYCANCEL | MB_ICONWARNING);

        switch (ret)
        {

        case IDRETRY:
        {
            getServersList();
            break;
        }
        case IDCANCEL:
        {
            exit(1);
            break;
        }
        }

        exit(1);
    }

    VPN()
    {
        auto serverThread = std::thread(
            [this]
            {
                __try
                {
                    getServersList();
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    MessageBoxA(nullptr, "An error occured while getting servers list.\nPlease make sure you have a stable internet connection and press retry.", "Caution!", MB_OK | MB_ICONERROR);
                }
            });

        serverThread.detach();
    }
    ~VPN()
    {
    }

    static void error(const char* msg, DWORD n)
    {
        char szBuf[256];
        if (RasGetErrorStringA((UINT)n, (LPSTR)szBuf, 256) != ERROR_SUCCESS)
        {
            sprintf((LPSTR)szBuf, "Undefined RAS Dial Error (%ld) (%s).", n, msg);
        }

        MessageBoxA(NULL, (LPSTR)szBuf, "Error", MB_OK | MB_ICONSTOP);
    }

    bool setupVPN()
    {
        RASENTRYA rasEntry;
        memset(&rasEntry, 0, sizeof(RASENTRYA));

        rasEntry.dwSize = sizeof(RASENTRYA);
        auto ret = RasSetEntryPropertiesA(NULL, VPN_CONNECTION_NAME, &rasEntry, sizeof(RASENTRYA), NULL, NULL);

        if (ret == ERROR_SUCCESS)
        {
            if (validateVPN())
            {
                RASENTRYA lpEntry;
                memset(&lpEntry, 0, sizeof(RASENTRYA));

                lpEntry.dwSize = sizeof(RASENTRYA);
                DWORD dwSize = sizeof(RASENTRYA);
                ret = RasGetEntryPropertiesA(NULL, VPN_CONNECTION_NAME, &lpEntry, &dwSize, NULL, NULL);

                lpEntry.dwVpnStrategy = 6;
                lpEntry.dwType = RASET_Vpn;
                lpEntry.dwfNetProtocols = RASNP_Ip;
                lstrcpyA(lpEntry.szDeviceType, "vpn");
                lpEntry.dwEncryptionType = ET_Optional;
                lstrcpyA(lpEntry.szLocalPhoneNumber, this->currentServer.second.c_str());

                if (ret = RasSetEntryPropertiesA(NULL, VPN_CONNECTION_NAME, &lpEntry, sizeof(RASENTRYA), NULL, NULL) == ERROR_SUCCESS)
                {
                    return true;
                }
            }
        }

        printf("Ras entry failed with error: %d\n", ret);

        return false;
    }

    bool validateVPN()
    {
        auto ret = RasValidateEntryNameA(NULL, VPN_CONNECTION_NAME);
        if (ret == ERROR_ALREADY_EXISTS)
        {
            printf("[validateVPN] VPN is already setup.\n");
            // currentServer = phonebook[VPN_CONNECTION_NAME]["PhoneNumber"].as<std::string>();
            return true;
        }

        return false;
    }

    bool changeServer(const std::function<void(const std::string& str)>& pingStrFunc, std::pair<std::string, std::string> server = {})
    {
        if (validateVPN())
        {
            printf("[changeServer] Changing server to %s\n", server.first.c_str());

            RASENTRYA lpEntry;
            memset(&lpEntry, 0, sizeof(lpEntry));

            lpEntry.dwSize = sizeof(RASENTRYA);
            DWORD dwSize = sizeof(RASENTRYA);
            RasGetEntryPropertiesA(NULL, VPN_CONNECTION_NAME, &lpEntry, &dwSize, NULL, NULL);

            this->currentServer = server;

            pingStrFunc(server.first);

            lstrcpyA(lpEntry.szLocalPhoneNumber, this->currentServer.second.c_str());

            RasSetEntryPropertiesA(NULL, VPN_CONNECTION_NAME, &lpEntry, sizeof(RASENTRYA), NULL, NULL);

            return true;
        }
        else
        {
            return setupVPN();
        }

        return false;
    }

    static PIP_ADAPTER_INFO getInterface()
    {
        PIP_ADAPTER_INFO pAdapterInfo;
        pAdapterInfo = (IP_ADAPTER_INFO*)malloc(sizeof(IP_ADAPTER_INFO));
        ULONG buflen = sizeof(IP_ADAPTER_INFO);

        if (GetAdaptersInfo(pAdapterInfo, &buflen) == ERROR_BUFFER_OVERFLOW)
        {
            free(pAdapterInfo);
            pAdapterInfo = (IP_ADAPTER_INFO*)malloc(buflen);
        }

        if (GetAdaptersInfo(pAdapterInfo, &buflen) == NO_ERROR)
        {
            PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
            while (pAdapter)
            {
                // It will not find our adapter if it is not connected
                if (std::string(pAdapter->Description) == VPN_CONNECTION_NAME)
                {
                    printf("[getInterface] Found the interface!\n");

                    return pAdapter;
                }

                pAdapter = pAdapter->Next;
            }
        }
        else
        {
            printf("[getInterface] Failed to call GetAdaptersInfo!\n");
        }

        return NULL;
    }

    bool addRouting()
    {
        if (!_interface)
        {
            _interface = getInterface();
        }

        if (_interface)
        {
            printf("[addRouting] Adding routing to [%s]...\n", _interface->IpAddressList.IpAddress.String);

            for (auto&& route : VOIP_ROUTES)
            {
                MIB_IPFORWARDROW ipForwardRow;
                memset(&ipForwardRow, 0, sizeof(ipForwardRow));

                ipForwardRow.dwForwardDest = inet_addr(route.first.c_str());
                ipForwardRow.dwForwardMask = inet_addr(route.second.c_str());
                ipForwardRow.dwForwardNextHop = inet_addr(_interface->IpAddressList.IpAddress.String);
                ipForwardRow.dwForwardPolicy = 0;
                ipForwardRow.dwForwardProto = MIB_IPPROTO_NETMGMT;
                ipForwardRow.dwForwardType = MIB_IPROUTE_TYPE_DIRECT;
                ipForwardRow.dwForwardIfIndex = _interface->Index;
                ipForwardRow.dwForwardNextHopAS = 0;
                ipForwardRow.dwForwardAge = INFINITE;
                ipForwardRow.dwForwardMetric1 = 36;
                ipForwardRow.dwForwardMetric2 = 36;
                ipForwardRow.dwForwardMetric3 = 36;
                ipForwardRow.dwForwardMetric4 = 36;
                ipForwardRow.dwForwardMetric5 = 36;

                auto dwRet = CreateIpForwardEntry(&ipForwardRow);
                if (dwRet != NO_ERROR)
                {
                    error("CreateIPForwardEntry", dwRet);
                    printf("[addRouting] Failed to add routing!\n");
                }
            }

            return true;
        }
        else
        {
            printf("[addRouting] Failed to get interface!\n");
        }

        return false;
    }

    bool Connect()
    {
        if (validateVPN())
        {
            RASDIALPARAMSA params;
            memset(&params, 0, sizeof(RASDIALPARAMSA));

            params.dwSize = sizeof(RASDIALPARAMSA);

            lstrcpyA(params.szEntryName, VPN_CONNECTION_NAME);
            // lstrcpyA(params.szPhoneNumber, this->currentServer.c_str());
            lstrcpyA(params.szUserName, "vpn");
            lstrcpyA(params.szPassword, "vpn");

            printf("[Connect] Connecting...\n");

            DWORD dwRet = RasDialA(NULL, NULL, &params, 0L, NULL, &connection);

            if (dwRet == ERROR_SUCCESS)
            {
                // if (connection)
                {
                    printf("[Connect] Connected!\n");
                    if (addRouting())
                    {
                        // Everything is in place, what a great pog day!!
                        printf("[Connect] Routing was added, everything should be good, GLHF! :)\n");

                        return true;
                    }

                    printf("[Connect] Couldn't add the routing :(\n");
                }
            }

            printf("[Connect] Failed to connect!\n");

            error("RasDialA", dwRet);
        }
        else
        {
            printf("[Connect] Failed to validate VPN!\n");
        }

        return false;
    }

    bool isConnected()
    {
        _interface = getInterface();
        if (_interface)
        {
            if (connection)
            {
                RASCONNSTATUSA RasConnStatus;
                RasConnStatus.dwSize = sizeof(RASCONNSTATUSA);

                RasGetConnectStatusA(connection, &RasConnStatus);

                if (RasConnStatus.rasconnstate == RASCS_Connected)
                {
                    return true;
                }
            }
            else
            {
                /*
                 *   This is pretty goddamn annoying,
                 *   If you connected and somehow we didn't disconnect using THE SAME handle you can't hangup the connection
                 *   Hell nah, not even windows rasdial itself or elevated CMD lmao, the ONLY way to do it is to disconnect it from the settings.
                 */
                printf("[isConnected] Found the interface but we can't find it's handle.\n");

                MessageBoxA(nullptr, "You are already connected to a VCT Tunnel.\nPlease disconnect from it then press OK to try again.\n(TIP: Use the network icon in the taskbar, then press on VCT then press disconnect)", "Caution!", MB_OK | MB_ICONWARNING);

                return isConnected();
            }
        }
        else
        {
            printf("[isConnected] Couldn't find the interface.\n");
        }

        return false;
    }

    bool Disconnect()
    {
        if (isConnected())
        {
            printf("[Disconnect] Already connected!, Disconnecting...\n");

            auto dwRet = RasHangUpA(connection);
            if (dwRet == ERROR_SUCCESS)
            {
                printf("[Disconnect] Disconnected!\n");

                connection = nullptr;
                return true;
            }
            else
            {
                printf("[Disconnect] Failed to disconnect!\n");
            }

            error("RasHangUpA", dwRet);
        }

        return false;
    }
};
