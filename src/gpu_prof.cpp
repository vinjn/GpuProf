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
#define GPU_PROF_VERSION "0.9 dev"

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

#include "screen_shot.h"

using namespace std;

#include "../3rdparty/CImg.h"
using namespace cimg_library;

bool isCanvasVisible = true;

vector<shared_ptr<CImgDisplay>> windows;

bool running = true;

int setup()
{
    system_setup();
    etw_setup();
    nvidia_setup();

    return 0;
}

int update()
{
    system_update();
    etw_update();
    nvidia_update();

    if (MetricsInfo::valid_element_count < MetricsInfo::HISTORY_COUNT)
        MetricsInfo::valid_element_count++;

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

// Application entry point
int main(int argc, char* argv[])
{
#if 0
    intel_main(0, NULL);
#endif

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

        ::Sleep(100);
    }

    cleanup();

    return 0;
}

