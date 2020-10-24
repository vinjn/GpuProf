#include <winsock2.h>
#include "gui_imgui.h"
#include "../3rdparty/imgui_remote/imgui_remote.h"

#pragma comment(lib, "Ws2_32")

static bool sTriggerNewFrame = false;
static INT64 g_TicksPerSecond = 0;
static INT64 g_Time = 0;
ImGuiContext* imguiCtx = NULL;

void createRemoteImgui(const char* address, int port)
{
    imguiCtx = ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    ImGui::RemoteInit(address, port);
    sTriggerNewFrame = true;

    ::QueryPerformanceFrequency((LARGE_INTEGER*)&g_TicksPerSecond);
    ::QueryPerformanceCounter((LARGE_INTEGER*)&g_Time);
}

void updateRemoteImgui()
{
    ImGui::RemoteUpdate();
    ImGui::RemoteInput input;
    if (ImGui::RemoteGetInput(input))
    {
        ImGuiIO& io = ImGui::GetIO();
        for (int i = 0; i < 256; i++)
            io.KeysDown[i] = input.KeysDown[i];
        io.KeyCtrl = input.KeyCtrl;
        io.KeyShift = input.KeyShift;
        io.MousePos = input.MousePos;
        io.MouseDown[0] = (input.MouseButtons & 1);
        io.MouseDown[1] = (input.MouseButtons & 2) != 0;
        io.MouseWheel += input.MouseWheelDelta;
        // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
        io.KeyMap[ImGuiKey_Tab] = ImGuiKey_Tab;
        io.KeyMap[ImGuiKey_LeftArrow] = ImGuiKey_LeftArrow;
        io.KeyMap[ImGuiKey_RightArrow] = ImGuiKey_RightArrow;
        io.KeyMap[ImGuiKey_UpArrow] = ImGuiKey_UpArrow;
        io.KeyMap[ImGuiKey_DownArrow] = ImGuiKey_DownArrow;
        io.KeyMap[ImGuiKey_Home] = ImGuiKey_Home;
        io.KeyMap[ImGuiKey_End] = ImGuiKey_End;
        io.KeyMap[ImGuiKey_Delete] = ImGuiKey_Delete;
        io.KeyMap[ImGuiKey_Backspace] = ImGuiKey_Backspace;
        io.KeyMap[ImGuiKey_Enter] = 13;
        io.KeyMap[ImGuiKey_Escape] = 27;
        io.KeyMap[ImGuiKey_A] = 'a';
        io.KeyMap[ImGuiKey_C] = 'c';
        io.KeyMap[ImGuiKey_V] = 'v';
        io.KeyMap[ImGuiKey_X] = 'x';
        io.KeyMap[ImGuiKey_Y] = 'y';
        io.KeyMap[ImGuiKey_Z] = 'z';
    }
}

void ImGui_ImplCinder_NewFrameGuard() {
    if (!sTriggerNewFrame)
        return;

    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.Fonts->IsBuilt()); // Font atlas needs to be built, call renderer _NewFrame() function e.g. ImGui_ImplOpenGL3_NewFrame() 

    // Setup display size
    io.DisplaySize = { 1024, 768 };

    // Setup time step
    INT64 current_time;
    ::QueryPerformanceCounter((LARGE_INTEGER*)&current_time);
    io.DeltaTime = (float)(current_time - g_Time) / g_TicksPerSecond;
    g_Time = current_time;

    ImGui::NewFrame();

    sTriggerNewFrame = false;
}

void ImGui_ImplCinder_PostDraw()
{
    ImGui::Render();
    auto draw_data = ImGui::GetDrawData();
    ImGui::RemoteDraw(draw_data->CmdLists, draw_data->CmdListsCount);
    sTriggerNewFrame = true;
}
