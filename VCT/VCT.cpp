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
#include "include/imgui/imgui_internal.h"

#include "ImTween.h"

#define DrawerColoredButton(name, width, height, smallText, fullText, textCol, col1, col2, rounding)  \
    static float name##ButtonWidth = width;                                                           \
    static std::string name##ButtonText = smallText;                                                  \
    auto name##Button = ImGui::ColoredButton(name##ButtonText.c_str(), { name##ButtonWidth, height }, \
        textCol,                                                                                      \
        col1,                                                                                         \
        col2, rounding);                                                                              \
                                                                                                      \
    auto name##ButtonIsHovered = ImGui::IsItemHovered();                                              \
                                                                                                      \
    name##ButtonText = std::format("{}", name##ButtonIsHovered ? smallText fullText : smallText);

constexpr auto WINDOW_TITLE = L"VCT (Made by kemo)";
constexpr auto WINDOW_WIDTH = 400;
constexpr auto WINDOW_HEIGHT = 400;

inline auto vpn = new VPN();

LONG CALLBACK CrashHandler(EXCEPTION_POINTERS* e)
{
    vpn->Disconnect();
    delete vpn;

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

    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, WINDOW_TITLE, NULL };
    wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON));

    auto Shell_TrayWnd = FindWindow(L"Shell_TrayWnd", NULL);
    auto TrayNotifyWnd = FindWindowEx(Shell_TrayWnd, NULL, L"TrayNotifyWnd", NULL);
    auto Button = FindWindowEx(TrayNotifyWnd, NULL, L"Button", NULL);

    RECT Rect;
    GetWindowRect(Button, &Rect);

    auto x = Rect.left + (Rect.right - Rect.left) / 2 - WINDOW_WIDTH / 2;
    auto y = Rect.top - WINDOW_HEIGHT + 10;

    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, WINDOW_TITLE, WS_TILED, x, y, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, wc.hInstance, NULL);

    BOOL USE_DARK_MODE = true;
    BOOL SET_IMMERSIVE_DARK_MODE_SUCCESS = SUCCEEDED(DwmSetWindowAttribute(
        hwnd, DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE,
        &USE_DARK_MODE, sizeof(USE_DARK_MODE)));

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

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

    ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeuib.ttf", 20.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

    // merge in icons
    static const ImWchar icons_ranges[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.GlyphOffset = { 0.f, 5.f };
    icons_config.PixelSnapH = true;
    icons_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void*)Resources::materialIcons, sizeof(Resources::materialIcons), 20.0f, &icons_config, icons_ranges);

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

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        static std::string curPing = "999";
        static auto curTab = 0;

        static auto statusText = "Not connected";
        static auto currColor = RedColor;
        static auto bStatus = false;

        auto windowSize = ImGui::GetWindowSize();
        auto viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(io.DisplaySize);

        ImGui::Begin("VCT", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

        /* static auto rectwidth = 65.f;
        auto rectpos = ImRect(viewport->WorkPos, { viewport->WorkPos.x + rectwidth, viewport->WorkPos.y + 360.f });

        ImGui::GetWindowDrawList()->AddRectFilled(rectpos.Min, rectpos.Max, ImColor(34, 36, 40, 255), 2.0f);*/

        ImGui::Indent(6);

        DrawerColoredButton(minimize, 50.f, 50.f,
            ICON_MD_MINIMIZE, " Minimize",
            IM_COL32(255, 255, 255, 255),
            IM_COL32(90, 86, 191, 255),
            IM_COL32(44, 42, 89, 255), 18.0f);

        ImTween<float>::Tween(std::tuple { 50.0f, 100.0f, &minimizeButtonWidth })
            .OfType(ImTween<>::TweenType::PingPong)
            .Speed(8.0f)
            .When(
                [&]()
                {
                    return minimizeButtonIsHovered;
                })
            .Tick();

        if (minimizeButton)
        {
            ShowWindow(hwnd, SW_HIDE);

            ShowTrayNotificationBalloon(hwnd, L"Your VPN is still working!", L"VCT is now minimized, access it from the notification tray.", NIIF_INFO);
        }

        DrawerColoredButton(close, 50.f, 50.f,
            ICON_MD_CLOSE, " Close",
            IM_COL32(255, 255, 255, 255),
            IM_COL32(90, 86, 191, 255),
            IM_COL32(44, 42, 89, 255), 18.0f);

        ImTween<float>::Tween(std::tuple { 50.0f, 100.0f, &closeButtonWidth })
            .OfType(ImTween<>::TweenType::PingPong)
            .Speed(8.0f)
            .When(
                [&]()
                {
                    return closeButtonIsHovered;
                })
            .Tick();

        if (closeButton)
        {
            exit(0);
        }

        DrawerColoredButton(support, 50.f, 50.f,
            ICON_MD_SUPPORT_AGENT, " Support",
            IM_COL32(255, 255, 255, 255),
            IM_COL32(90, 86, 191, 255),
            IM_COL32(44, 42, 89, 255), 18.0f);

        ImTween<float>::Tween(std::tuple { 50.0f, 100.0f, &supportButtonWidth })
            .OfType(ImTween<>::TweenType::PingPong)
            .Speed(8.0f)
            .When(
                [&]()
                {
                    return supportButtonIsHovered;
                })
            .Tick();

        if (supportButton)
        {
            ShellExecute(0, 0, L"https://discord.gg/5WEPu6tSvA", 0, 0, SW_SHOW);
        }

        ImGui::Indent(150);

        ImGui::SetCursorPosY(8);

        ImGui::BeginGroup();
        {
            if (ImGui::SwitchButton("##switch", &bStatus))
            {
                if (!bStatus)
                {
                    currColor = RedColor;
                    statusText = "Disconnecting...";

                    std::thread disconnectThread(
                        []()
                        {
                            auto ret = vpn->Disconnect();

                            if (ret)
                            {
                                currColor = RedColor;
                                statusText = "Disconnected";
                            }
                            else
                            {
                                currColor = YellowColor;
                                statusText = " Connected";
                                // ImGui::Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImVec4(1.00f, 1.00f, 1.00f, 1.00f)));
                            }
                        });

                    disconnectThread.detach();
                }
                else
                {
                    currColor = RedColor;
                    statusText = "Connecting...";

                    std::thread connectThread(
                        []()
                        {
                            auto ret = vpn->Connect();

                            if (!ret)
                            {
                                currColor = RedColor;
                                statusText = "Disconnected";
                            }
                            else
                            {
                                currColor = YellowColor;
                                statusText = " Connected";
                                // ImGui::Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImVec4(1.00f, 1.00f, 1.00f, 1.00f)));
                            }
                        });

                    connectThread.detach();
                }
            }

            ImGui::SetWindowFontScale(1.2f);

            ImGui::TextColored(currColor, statusText);

            ImGui::SetWindowFontScale(1.0f);

            ImGui::EndGroup();
        }

        // ImGui::SameLine();

        auto pingStrLamb = [&](const std::string& str)
        {
            auto p = util::ping(str.c_str());

            if (p != 999)
            {
                curPing = std::to_string(p);
            }
            else
            {
                MessageBoxW(nullptr, L"This server is unavailable, please select a different one\nالسيرفر دا مش متاح, اختار واحد تاني", L"Error", MB_OK);
                curPing = "N/A (Don't use!)";
            }
        };

        /* if (ImGui::ColoredButton(ICON_MD_BAR_CHART,
                ImVec2(50.0f, 50.0f),
                IM_COL32(255, 255, 255, 255),
                IM_COL32(110, 93, 208, 255),
                IM_COL32(98, 92, 166, 255), 18.0f))
        {
            // pingStrLamb(vpn->currentServer);
        }
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), curPing.c_str());

        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 30.0f);
        ImGui::TextStyled("ms", 20.0f, IM_COL32(255, 255, 255, 255));*/

        /*if (ImGui::ColoredButton("\xee\x99\xa3 Auto", ImVec2(180.0f, 50.0f), IM_COL32(255, 255, 255, 255), IM_COL32(254, 208, 1, 255), IM_COL32(253, 104, 1, 255)))
        {
            vpn->changeServer(pingStrLamb);
        }*/
        // ImGui::SameLine();
        // ImGui::HelpMarker("Automatically connect to the best server (ping wise)");

        if (vpn->serversList.size() == 0)
        {
            ImGui::Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImVec4(1.00f, 1.00f, 1.00f, 1.00f)));
        }

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 40);

        if (ImGui::BeginChild("#servers", ImVec2(0, 0), false))
        {
            ImGui::SetWindowFontScale(0.9f);

            for (auto&& p : vpn->serversList)
            {
                // auto s = p.first + "\t\t\t\xEE\x97\x8C";

                if (ImGui::ColoredButton(p.first.c_str(),
                        ImVec2(200.0f, 50.0f),
                        IM_COL32(255, 255, 255, 255),
                        IM_COL32(111, 107, 242, 255),
                        IM_COL32(66, 63, 140, 255), 18.0f))
                {
                    vpn->changeServer(pingStrLamb, p);
                }

                ImGui::Spacing();
            }

            ImGui::EndChild();

            ImGui::SetWindowFontScale(1.0f);
        }

        ImGui::End();

        if (IsWindowVisible(hwnd))
        {
            ImGui::Render();
            float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, color);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            g_pSwapChain->Present(1, 0); // Present with vsync
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 60));
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
