
#include "client.h"
#include "guiHelpers.h"
#include "resources/resources.h"
#include "vpn.h"

constexpr auto WINDOW_TITLE = L"VCT";
constexpr auto WINDOW_WIDTH = 700;
constexpr auto WINDOW_HEIGHT = 500;
constexpr auto WINDOW_X = 100;
constexpr auto WINDOW_Y = 100;

inline auto vpn = new VPN();
inline auto cli = new Client();

// Main code
int main(int, char**)
{
    SetUnhandledExceptionFilter(util::unhandled_handler);

    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, WINDOW_TITLE, NULL };
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow(wc.lpszClassName, WINDOW_TITLE, WS_OVERLAPPEDWINDOW, WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL, wc.hInstance, NULL);

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) & ~WS_SIZEBOX);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImGui::CustomTheme();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\bahnschrift.ttf", 25.0f, NULL, io.Fonts->GetGlyphRangesJapanese());

    // merge in icons
    static const ImWchar icons_ranges[] = { ICON_MIN_MD, ICON_MAX_MD, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.GlyphOffset = { 0.f, 5.f };
    icons_config.PixelSnapH = true;
    icons_config.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void*)Resources::materialIcons, sizeof(Resources::materialIcons), 25.0f, &icons_config, icons_ranges);

    std::vector<ID3D11ShaderResourceView*> rankTextures;

    for (auto&& d : Resources::rankBuffers)
    {
        ID3D11ShaderResourceView* texture = NULL;

        if (ImGui::LoadTextureFromBuffer(d.first, d.second, &texture))
        {
            rankTextures.push_back(texture);
        }
    }

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

        static std::string curPing;
        static auto curTab = 0;
        static auto bIsMenuOpen = true;

        static auto statusText = disconnected_str;
        static auto bStatus = false;
        static auto currColor = RedColor;
        {
            auto windowSize = ImGui::GetWindowSize();

            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(io.DisplaySize);

            ImGui::Begin("VCT", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

            auto gameState = cli->getGameState();

            if (gameState != cli->currentGameState)
            {
                cli->onStateChanged(gameState);
            }

            /*ImGui::GetWindowDrawList()->AddRectFilled({ 150.0f, 0 }, { 800.f, 455.f }, IM_COL32(16, 18, 24, 255), 20.0f, ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft);

            if (bIsMenuOpen)
            {
                ImGui::GetWindowDrawList()->AddRectFilled({ 0.f, (curTab + 1) * 30.f }, { 150.0f, (curTab + 1) * 85.f }, IM_COL32(16, 18, 24, 255), 30.0f, ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomLeft);
            }*/

            /*
            if (ImGui::ClickableText(ICON_MD_MENU))
            {
                bIsMenuOpen = !bIsMenuOpen;
            }
            */

            ImGui::NewLine();

            ImGui::SetWindowFontScale(1.1f);

            if (bIsMenuOpen)
            {

                if (ImGui::ClickableText("\xEE\xA2\x8D Main"))
                {
                    curTab = 0;
                }

                ImGui::NewLine();

                if (ImGui::ClickableText("\xEE\xA3\x9E Servers"))
                {
                    curTab = 1;
                }

                ImGui::NewLine();

                if (ImGui::ClickableText("\xEE\xA2\xB8 Settings"))
                {
                    curTab = 2;
                }

                ImGui::NewLine();

                if (ImGui::ClickableText("\xEE\xA2\x8E About"))
                {
                    curTab = 3;
                }

                ImGui::NewLine();

                if (ImGui::ClickableText("\xEE\xA2\x8E Test"))
                {
                    curTab = 4;
                }
            }

            ImGui::SetWindowFontScale(1.0f);

            ImGui::SetCursorPosX(windowSize.x * 0.7f);
            ImGui::SetCursorPosY(windowSize.y * 0.2f);

            auto pingStrLamb = [&](const std::string& str)
            {
                auto p = util::ping(str.c_str());

                if (p != 999)
                {
                    curPing = std::to_string(p);
                }
                else
                {
                    curPing = "N/A (Don't use!)";
                }
            };

            switch (curTab)
            {

            case 0:
            {
                ImGui::BeginGroup();
                {
                    // IM_COL32(85, 206, 199, 255), IM_COL32(22, 103, 219, 255)
                    // ImGui::ColoredButton("00:00", ImVec2(300.0f, 70.0f), IM_COL32(255, 255, 255, 255), IM_COL32(50, 75, 217, 255), IM_COL32(78, 99, 217, 255));

                    //ImGui::Checkbox("Demo Window", &show_demo_window);

                    if (ImGui::SwitchButton("##switch", &bStatus))
                    {
                        if (!bStatus)
                        {
                            vpn->Disconnect();
                        }
                        else
                        {
                            auto ret = vpn->Connect();
                            if (ret)
                            {
                                PlaySound((LPCWSTR)Resources::successSound, nullptr, SND_MEMORY | SND_ASYNC);
                            }
                            else
                            {
                                PlaySound((LPCWSTR)Resources::failedSound, nullptr, SND_MEMORY | SND_ASYNC);
                            }

                            if (!ret)
                            {
                                currColor = RedColor;
                                statusText = disconnected_str;
                            }
                            else
                            {
                                currColor = GreenColor;
                                statusText = connected_str;
                                ImGui::Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImVec4(1.00f, 1.00f, 1.00f, 1.00f)));
                            }
                        }
                    }

                    ImGui::TextColored(currColor, statusText);

                    ImGui::EndGroup();
                }
                break;
            }

            case 1:
            {
                if (ImGui::ColoredButton(ICON_MD_BAR_CHART, ImVec2(50.0f, 50.0f), IM_COL32(255, 255, 255, 255), IM_COL32(36, 196, 191, 255), IM_COL32(40, 115, 228, 255)))
                {
                    pingStrLamb(vpn->currentServer);
                }
                ImGui::SameLine();
                ImGui::SetCursorPosX(windowSize.x * 0.7f + 60.0f);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), curPing.c_str());

                ImGui::SameLine();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
                ImGui::TextStyled("ms", 15.0f, IM_COL32(255, 255, 255, 255));

                ImGui::SetCursorPosX(windowSize.x * 0.7f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
                ImGui::SameLine();

                ImGui::SetCursorPosX(windowSize.x * 0.7f + 60.0f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20.0f);
                ImGui::TextStyled("PING", 15.0f, IM_COL32(255, 255, 255, 255));

                // ImGui::SetCursorPosX(windowSize.x * 0.7f + 140.0f);
                // ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 85.0f);

                ImGui::SetCursorPosX(windowSize.x * 0.7f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 20.0f);

                /*if (ImGui::ColoredButton("\xee\x99\xa3 Auto", ImVec2(180.0f, 50.0f), IM_COL32(255, 255, 255, 255), IM_COL32(254, 208, 1, 255), IM_COL32(253, 104, 1, 255)))
                {
                    vpn->changeServer(pingStrLamb);
                }*/
                ImGui::SameLine();
                ImGui::HelpMarker("Automatically connect to the best server (ping wise)");

                ImGui::SetCursorPosX(windowSize.x * 0.7f);

                if (vpn->serversList.size() == 0)
                {
                    ImGui::SetCursorPosX(windowSize.x * 0.7f + 20.0f);
                    ImGui::Spinner("##spinner", 15, 6, ImGui::GetColorU32(ImVec4(1.00f, 1.00f, 1.00f, 1.00f)));
                }

                if (ImGui::BeginChild("#servers", ImVec2(0, 0), false))
                {
                    ImGui::SetWindowFontScale(0.9f);

                    for (auto&& p : vpn->serversList)
                    {
                        // auto s = p.first + "\t\t\t\xEE\x97\x8C";

                        if (ImGui::ColoredButton(p.first.c_str(), ImVec2(200.0f, 50.0f), IM_COL32(255, 255, 255, 255), IM_COL32(78, 99, 217, 255), IM_COL32(50, 75, 217, 255)))
                        {
                            vpn->changeServer(pingStrLamb, p);
                        }

                        ImGui::Spacing();
                    }

                    ImGui::EndChild();

                    ImGui::SetWindowFontScale(1.0f);
                }
                break;
            }

            case 2:
                ImGui::Text("Settings");
                break;

            case 4:
            {
                ImGui::SetWindowFontScale(0.9f);

                if (cli->stats.size() > 0)
                {
                    static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

                    ImVec2 outer_size = ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8);
                    if (ImGui::BeginTable("table_scrolly", 4, flags, outer_size))
                    {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("Rank", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("RR", ImGuiTableColumnFlags_None);
                        ImGui::TableSetupColumn("Leaderboard", ImGuiTableColumnFlags_None);
                        ImGui::TableHeadersRow();

                        for (int i = 0; i < cli->stats.size(); i++)
                        {
                            ImGui::TableNextRow();
                            auto [name, rank, rr, leaderboard] = cli->stats[i];
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text(name.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Image((void*)rankTextures[rank], ImVec2(50, 50));
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%d", rr);
                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%d", leaderboard);
                        }

                        ImGui::EndTable();
                    }
                }

                ImGui::SetWindowFontScale(1.0f);
            }
            }

            ImGui::End();
        }

        ImGui::Render();
        float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 70));
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
