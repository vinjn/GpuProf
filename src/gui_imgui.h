#pragma once

#include "../3rdparty/imgui/imgui.h"
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

void createRemoteImgui(const char* address, int port = 7002);
void updateRemoteImgui(bool ENABLE_REMOTE_GUI);

void ImGui_ImplCinder_NewFrameGuard();
void ImGui_ImplCinder_PostDraw(bool ENABLE_REMOTE_GUI, bool ENABLE_LOCAL_GUI);
