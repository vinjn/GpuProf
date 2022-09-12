/*
 * MIT License
 *
 * Copyright (c) 2016 Jeremy Main (jmain.jp@gmail.com)
 * Copyright (c) 2019-2020 Vinjn Zhang (vinjn@qq.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _HAS_STD_BYTE 0
#define GPU_PROF_VERSION "1.3"

#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <assert.h>
#include <memory>
#include <string>

#include "nvidia_prof.h"
#include "etw_prof.h"
#include "system_prof.h"
#include "metrics_info.h"
#include "gui_imgui.h"

// TODO: cross-platform
#include "../build/resource.h"

#include "screen_shot.h"

#include "shlwapi.h"

using namespace std;

#include "../3rdparty/CImg.h"
using namespace cimg_library;

bool isCimgVisible = false;
bool isImguiEnabled = false;
bool isRemoteGuiEnabled = false;

vector<shared_ptr<CImgDisplay>> windows;

bool running = true;

int setup()
{
    system_setup();
    etw_setup();
    nvidia_setup();

    for (auto& window : windows)
    {
        HICON hIcon = LoadIcon(GetModuleHandle("gpuprof.exe"), MAKEINTRESOURCE(IDI_ICON1));
        SendMessage(window->_window, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        SendMessage(window->_window, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }

    return 0;
}

int update()
{
    system_update();
    etw_update();
    nvidia_update();

    if (isImguiEnabled)
    {
        if (!updateImgui())
            return 1;
    }

    // GUI
#ifdef WIN32_WITH_THIS
    SHORT state = GetAsyncKeyState(VK_SPACE);
    if (state & 1)
    {
        // LSB of state indicates it's a "CLICK"
        isCimgVisible = !isCimgVisible;
        if (isCimgVisible) window->show();
        else window->close();
    }
#endif

    return 0;
}

int cleanup()
{
    etw_cleanup();
    nvidia_cleanup();

    if (isImguiEnabled)
        destroyImgui();

    return 0;
}

int global_mouse_x = -1;
int global_mouse_y = -1;
char exe_folder[MAX_PATH + 1] = "";

void drawCimg()
{
    global_mouse_x = -1;
    global_mouse_y = -1;

    for (auto& window : windows)
    {
        auto xm = window->mouse_x();
        auto ym = window->mouse_y();
        if (xm >= 0 && ym >= 0)
        {
            global_mouse_x = xm;
            global_mouse_y = ym;
        }
    }

    if (global_mouse_x < 0 || global_mouse_y < 0)
    {
        global_mouse_x = windows[0]->width() - 1;
        global_mouse_y = 0;
    }

    int idx = 0;
    int x0 = windows[0]->window_x();
    int y0 = windows[0]->window_y();

    bool force_show_window = false;
    bool capture_etl = false;

    for (auto& window : windows)
    {
        if (window->is_keyESC()) running = false;
        if (window->is_keySPACE()) force_show_window = true;
        if (window->is_keyF8()) capture_etl = true;

        window->move(x0, y0 + idx * (WINDOW_H + 32));

        if (idx == 0)
        {
            system_draw();
        }
        else if (idx == 1)
        {
            etw_draw();
        }
        else
        {
            nvidia_draw();
        }

        idx++;
    }

    for (auto& window : windows)
    {
        if (force_show_window)
            window->show();
    }

    if (capture_etl)
    {
        char buf[256];
        sprintf(buf, "sudo.exe %s\\etw\\capture.bat", exe_folder);
        system(buf);
    }
}

void drawImgui(bool isRemote)
{
    if (isRemote)
    {
        updateRemoteImgui();
        ImGui_ImplCinder_NewFrameGuard();
    }
    else
    {
        ImGui_SDL_BeginDraw();
    }

    static bool showImguiDemoWindow = false;
    if (ImGui::Button("showImguiDemoWindow"))
        showImguiDemoWindow = !showImguiDemoWindow;
    ImGui::ShowDemoWindow(&showImguiDemoWindow);
    static bool showImplotDemoWindow = false;
    if (ImGui::Button("showImplotDemoWindow"))
        showImplotDemoWindow = !showImplotDemoWindow;
    ImPlot::ShowDemoWindow(&showImplotDemoWindow);

    //ImGui::SetNextWindowPos(ImVec2(0, 0));
    //ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("GpuProf " GPU_PROF_VERSION " from vinjn.con");

    system_draw_imgui();
    etw_draw_imgui();
    nvidia_draw_imgui();

    ImGui::End();

    if (isRemote)
    {
        ImGui_ImplCinder_PostDraw();
    }
    else
    {
        ImGui_SDL_EndDraw();
    }
}

// Application entry point
int main(int argc, char* argv[])
{
#if 0
    intel_main(0, NULL);
#endif

    if (argc >= 2)
    {
        char* addr = argv[1];
        if (strcmp(addr, "-zen") == 0)
        {
        }
        else if (strcmp(addr, "-remote") == 0)
        {
            isRemoteGuiEnabled = true;

            createRemoteImgui(addr);
        }
        if (strcmp(addr, "-imgui") == 0)
        {
            isImguiEnabled = true;

            createImgui();
        }
        else
        {
            isCimgVisible = true;
        }
    }
    else
    {
        isCimgVisible = true;
    }

    GetModuleFileNameA(NULL, exe_folder, MAX_PATH);
    PathRemoveFileSpecA(exe_folder);

    //gdiscreen();

    printf("GpuProf %s from vinjn.com\n", GPU_PROF_VERSION);

    if (setup() != 0)
        return -1;

    while (running)
    {
        if (update() != 0)
            break;

        if (isCimgVisible)
        {
            drawCimg();
        }
        if (isRemoteGuiEnabled)
        {
            drawImgui(true);
        }
        if (isImguiEnabled)
        {
            drawImgui(false);
        }
        ::Sleep(100);
    }

    cleanup();

    return 0;
}

