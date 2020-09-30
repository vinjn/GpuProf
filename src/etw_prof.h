#pragma once

#include "../3rdparty/PresentMon/PresentData/PresentMonTraceConsumer.hpp"

void ElevatePrivilege(int argc, char** argv);
bool etw_setup();
void etw_update();
void etw_quit();
