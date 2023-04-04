#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCKAPI_
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <ras.h>
#include <windows.h>
#include <ipexport.h>
#include <format>
#include <dwmapi.h>

#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "Rasapi32.lib")
#pragma comment(lib, "Dbghelp.lib")

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32")

#include "guiHelpers.h"
#include "resources/resources.h"
#include "vpn.h"

constexpr auto WINDOW_TITLE = L"VCT (Made by kemo)";
constexpr auto WINDOW_WIDTH = 400;
constexpr auto WINDOW_HEIGHT = 400;

inline auto vpn = new VPN();

constexpr auto VERSION = 2.1;

inline auto CheckForUpdate()
{
    httplib::Client cli("https://kem.ooo");

    auto r = cli.Get("/VCT");

    if (r->status == 200)
    {
        auto version = std::stof(r->body);

        if (version > VERSION)
        {
            MessageBoxA(nullptr, "A new version of VCT is available!", "VCT", MB_OK | MB_ICONINFORMATION);
        }
    }
}

LONG CALLBACK CrashHandler(EXCEPTION_POINTERS* e)
{
    util::make_minidump(e);
    return EXCEPTION_CONTINUE_SEARCH;
}

// Main code
int main(int, char**)
{
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    (void)freopen("log.txt", "w", stdout);

    SetUnhandledExceptionFilter(CrashHandler);

    std::atexit(
        []()
        {
            vpn->Disconnect();
            delete vpn;
        });

    std::thread updateThread(CheckForUpdate);
    updateThread.detach();

    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, WINDOW_TITLE, NULL };
    wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));

    auto Shell_TrayWnd = FindWindow(L"Shell_TrayWnd", NULL);
    auto TrayNotifyWnd = FindWindowEx(Shell_TrayWnd, NULL, L"TrayNotifyWnd", NULL);
    auto Button = FindWindowEx(TrayNotifyWnd, NULL, L"Button", NULL);

    RECT Rect;
    GetWindowRect(Button, &Rect);

    auto x = Rect.left + (Rect.right - Rect.left) / 2 - WINDOW_WIDTH / 2;
    auto y = Rect.top - WINDOW_HEIGHT;

    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, WINDOW_TITLE, WS_OVERLAPPED, x, y, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, wc.hInstance, NULL);

    // BOOL USE_DARK_MODE = true;
    // DwmSetWindowAttribute(hwnd, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE, &USE_DARK_MODE, sizeof(USE_DARK_MODE));

    SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) & ~WS_CAPTION);

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // sorry it's the only way i could think of
    std::thread trayThread(
        [&]()
        {
            while (!ShowTrayIcon(hwnd))
            {
            };
        });

    trayThread.detach();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImGui::CustomTheme();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ImFont* font = io.Fonts->AddFontFromMemoryCompressedTTF(
        Resources::InterRegular_compressed_data,
        Resources::InterRegular_compressed_size,
        20.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

    // merge in icons
    static const ImWchar icons_ranges[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.GlyphOffset = { 0.f, 5.f };
    icons_config.PixelSnapH = true;
    icons_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void*)Resources::materialIcons, sizeof(Resources::materialIcons), 20.0f, &icons_config, icons_ranges);

    ImFont* boldFont = io.Fonts->AddFontFromMemoryCompressedTTF(
        Resources::InterBold_compressed_data,
        Resources::InterBold_compressed_size,
        25.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

    // Our state
    bool show_demo_window = false;
    bool showServersWindow = false;

    // Main loop
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        if (isMinimized)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        static auto curTab = 0;

        static std::string statusText = "NOT CONNECTED";
        static std::string serverText = "--------";

        static auto currColor = DisconnectedColor;
        static auto connectionButtonStatus = false;

        auto windowSize = ImGui::GetWindowSize();
        auto viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGui::Begin("VCT", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

        // draw rect at the top of the view port
        auto rectPos = ImRect(viewport->Pos.x, viewport->Pos.y, viewport->Pos.x + viewport->Size.x, viewport->Pos.y + 37);

        ImGui::GetWindowDrawList()->AddRectFilled(rectPos.Min, rectPos.Max, ImColor(28, 36, 49, 255), 2.0f);

        ImGui::SetCursorPosY(8);
        ImGui::SetCursorPosX(10);

        ImGui::TextColored({ 0.498f, 0.518f, 0.545f, 1.0f }, "VCT | Made by Kemo");

        ImGui::SameLine();

        ImGui::Indent(320);

        if (ImGui::ClickableText(ICON_MD_HEADPHONES))
        {
            // ShellExecute(0, 0, L"https://discord.gg/5WEPu6tSvA", 0, 0, SW_SHOW);
            system("start https://discord.gg/5WEPu6tSvA");
        }

        ImGui::HelpMarker("Join our discord server for help");

        ImGui::SameLine();

        if (ImGui::ClickableText(ICON_MD_REMOVE))
        {
            isMinimized = true;

            ShowWindow(hwnd, SW_HIDE);

            ShowTrayNotificationBalloon(hwnd, L"Your VPN is still working!", L"VCT is now minimized, access it from the notification tray.", NIIF_INFO);
        }

        ImGui::HelpMarker("Minimize");

        ImGui::SameLine();

        if (ImGui::ClickableText(ICON_MD_CLOSE))
        {
            exit(0);
        }

        ImGui::HelpMarker("Exit");

        ImGui::Indent(-270);

        ImGui::SetCursorPosY(55);

        if (ImGui::BeginChild("#topPart", ImVec2(0, 130), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (ImGui::PowerButton("#connectButton",
                    ImVec2(120.0f, 120.0f),
                    IM_COL32(28, 36, 49, 255),
                    connectionButtonStatus ? IM_COL32(131, 255, 136, 255) : IM_COL32(255, 89, 89, 255),
                    60.0f, &connectionButtonStatus))
            {
                if (!connectionButtonStatus)
                {
                    currColor = DisconnectedColor;
                    statusText = "DISCONNECTING";

                    std::thread disconnectThread(
                        []()
                        {
                            auto ret = vpn->Disconnect();

                            if (ret)
                            {
                                currColor = DisconnectedColor;
                                statusText = "DISCONNECTED";
                                serverText = "--------";
                            }
                            /*
                            else
                            {
                                currColor = ConnectedColor;
                                statusText = "CONNECTED";
                                // ImGui::Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImVec4(1.00f, 1.00f, 1.00f, 1.00f)));
                            }
                            */
                        });

                    disconnectThread.detach();
                }
                else
                {
                    currColor = ConnectedColor;
                    statusText = "CONNECTING";

                    std::thread connectThread(
                        []()
                        {
                            auto ret = vpn->Connect();

                            if (!ret)
                            {
                                currColor = DisconnectedColor;
                                statusText = "DISCONNECTED";
                            }
                            else
                            {
                                currColor = ConnectedColor;
                                statusText = "CONNECTED";
                                serverText = vpn->currentServer.first.c_str();
                                // ImGui::Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImVec4(1.00f, 1.00f, 1.00f, 1.00f)));
                            }
                        });

                    connectThread.detach();
                }
            }

            ImGui::SameLine();
            ImGui::SetCursorPosY(35);

            ImGui::PushFont(boldFont);
            ImGui::TextColored(currColor, statusText.c_str());
            ImGui::PopFont();

            ImGui::SetCursorPosX(127);
            ImGui::SetCursorPosY(60);

            ImGui::Text(serverText.c_str());
        }

        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Indent(50);

        ImGui::SetWindowFontScale(1.0f);

        auto pingStrLamb = [&](const std::string& str)
        {
            auto p = util::ping(str.c_str());

            if (p == 999)
            {
                MessageBoxW(nullptr, L"This server is unavailable, please select a different one\nالسيرفر دا مش متاح, اختار واحد تاني", L"Error", MB_OK);
            }
        };

        if (vpn->serversList.size() == 0)
        {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 70);

            ImGui::Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImVec4(1.00f, 1.00f, 1.00f, 1.00f)));
        }
        else
        {
            if (ImGui::BeginChild("#servers", ImVec2(0, windowSize.y - 160), false))
            {
                ImGui::SetWindowFontScale(0.9f);

                for (auto&& p : vpn->serversList)
                {
                    static auto buttonColorDisabled = IM_COL32(28, 36, 49, 255);
                    static auto buttonColorActive = IM_COL32(63, 83, 115, 255);

                    bool isActive = vpn->currentServer == p;

                    if (ImGui::ColoredButton(p.first.c_str(),
                            ImVec2(200.0f, 50.0f),
                            IM_COL32(255, 255, 255, 255),
                            isActive ? buttonColorActive : buttonColorDisabled,
                            isActive ? buttonColorActive : buttonColorDisabled,
                            18.0f))
                    {
                        vpn->changeServer(pingStrLamb, p);
                    }

                    ImGui::Spacing();
                }

                ImGui::EndChild();

                ImGui::SetWindowFontScale(1.0f);
            }
        }

        ImGui::End();

        ImGui::Render();
        static float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 30));
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}
