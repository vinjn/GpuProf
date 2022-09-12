#pragma once

#include "imgui/imgui.h"
#include "implot/implot.h"
/*
item.def
    ITEM_DEF(string, REMOTE_GUI_IP, "127.0.0.1")
    ITEM_DEF(bool, ENABLE_REMOTE_GUI, false)
    ITEM_DEF(bool, ENABLE_LOCAL_GUI, true)

setup():
    createConfigImgui(getWindow(), false, false);
    createRemoteImgui(REMOTE_GUI_IP.c_str());

    getSignalUpdate().connect([&] {
        updateRemoteImgui(ENABLE_REMOTE_GUI);
        ImGui_ImplCinder_NewFrameGuard(getWindow());
        vnm::drawMinicofigImgui(true);
    });

    getWindow()->getSignalDraw().connect([&] {
        gl::clear();

        ImGui_ImplCinder_PostDraw(ENABLE_REMOTE_GUI, ENABLE_LOCAL_GUI);
    });
*/

bool createRemoteImgui(const char* address = "127.0.0.1", int port = 7003);
void updateRemoteImgui();
void ImGui_ImplCinder_NewFrameGuard();
void ImGui_ImplCinder_PostDraw();

bool createImgui();
void updateImgui();
void ImGui_SDL_BeginDraw();
void ImGui_SDL_EndDraw();
void destroyImgui();
