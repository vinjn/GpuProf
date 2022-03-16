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
#define GPU_PROF_VERSION "1.0"

#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <assert.h>
#include <memory>
#include <string>

#include "nvidia_prof.h"
#include "intel_prof.h"
#include "amd_prof.h"
#include "etw_prof.h"
#include "system_prof.h"
#include "metrics_info.h"
#include "gui_imgui.h"

// TODO: cross-platform
#include "../build/resource.h"

#include "screen_shot.h"

using namespace std;

#include "../3rdparty/CImg.h"
using namespace cimg_library;

bool isCanvasVisible = true;
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

    // GUI
#ifdef WIN32_WITH_THIS
    SHORT state = GetAsyncKeyState(VK_SPACE);
    if (state & 1)
    {
        // LSB of state indicates it's a "CLICK"
        isCanvasVisible = !isCanvasVisible;
        if (isCanvasVisible) window->show();
        else window->close();
    }
#endif

    return 0;
}

int cleanup()
{
    etw_cleanup();
    nvidia_cleanup();

    return 0;
}

int global_mouse_x = -1;
int global_mouse_y = -1;

void draw()
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
    int idx = 0;
    int x0 = windows[0]->window_x();
    int y0 = windows[0]->window_y();
    for (auto& window : windows)
    {
        if (window->is_keyESC()) running = false;
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
}

void drawImgui()
{
    updateRemoteImgui();
    ImGui_ImplCinder_NewFrameGuard();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1024, 768));
    ImGui::Begin("GpuProf " GPU_PROF_VERSION " from vinjn.con");

    system_draw_imgui();
    etw_draw_imgui();
    nvidia_draw_imgui();

    ImGui::End();

    ImGui_ImplCinder_PostDraw();
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
            isCanvasVisible = false;
        }
        else
        {
            isRemoteGuiEnabled = true;
            isCanvasVisible = false;

            createRemoteImgui(addr);
        }
    }

    //gdiscreen();

    printf("GpuProf %s from vinjn.com\n", GPU_PROF_VERSION);

    if (setup() != 0)
        return -1;

    while (running)
    {
        if (update() != 0)
            return -1;

        if (isCanvasVisible)
        {
            draw();
        }
        if (isRemoteGuiEnabled)
        {
            drawImgui();
        }

        ::Sleep(100);
    }

    cleanup();

    return 0;
}

