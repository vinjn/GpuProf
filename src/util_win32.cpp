#include "util_win32.h"

#if 0
#include "C:/Program Files (x86)/Windows Kits/10/Include/10.0.18362.0/km/d3dkmthk.h"

DWORD WINAPI vsyncThread(LPVOID lpParam) {
    printf("+vsyncThread()\n");
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    LPFND3DKMT_OPENADAPTERFROMHDC lpfnKTOpenAdapterFromHdc = (LPFND3DKMT_OPENADAPTERFROMHDC)fnBind("gdi32", "D3DKMTOpenAdapterFromHdc");
    LPFND3DKMT_WAITFORVERTICALBLANKEVENT lpfnKTWaitForVerticalBlankEvent = (LPFND3DKMT_WAITFORVERTICALBLANKEVENT)fnBind("gdi32", "D3DKMTWaitForVerticalBlankEvent");
    if (lpfnKTOpenAdapterFromHdc && lpfnKTWaitForVerticalBlankEvent) {
        D3DKMT_WAITFORVERTICALBLANKEVENT we;
        bool bBound = false;
        while (bRunningTests) {
            if (!bBound) {
                D3DKMT_OPENADAPTERFROMHDC oa;
                oa.hDc = GetDC(NULL);  // NULL = primary display monitor; NOT tested with multiple monitor setup; tested/works with hAppWnd
                bBound = (S_OK == (*lpfnKTOpenAdapterFromHdc)(&oa));
                if (bBound) {
                    we.hAdapter = oa.hAdapter;
                    we.hDevice = 0;
                    we.VidPnSourceId = oa.VidPnSourceId;
                }
            }
            if (bBound && (S_OK == (*lpfnKTWaitForVerticalBlankEvent)(&we))) {
                vsyncHaveNewTimingInfo(tick());
                vsyncSignalAllWaiters();
            }
            else {
                bBound = false;
                printf("*** vsync service in recovery mode...\n");
                Sleep(1000);
            }
        }
    }
    printf("-vsyncThread()\n");
    return 0;
}
#endif

void GoToXY(int column, int line)
{
    // Create a COORD structure and fill in its members.
    // This specifies the new position of the cursor that we will set.
    COORD coord;
    coord.X = column;
    coord.Y = line;

    // Obtain a handle to the console screen buffer.
    // (You're just using the standard console, so you can use STD_OUTPUT_HANDLE
    // in conjunction with the GetStdHandle() to retrieve the handle.)
    // Note that because it is a standard handle, we don't need to close it.
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // Finally, call the SetConsoleCursorPosition function.
    if (!SetConsoleCursorPosition(hConsole, coord))
    {
        // Uh-oh! The function call failed, so you need to handle the error.
        // You can call GetLastError() to get a more specific error code.
        // ...
    }
}

PROCESSENTRY32 getEntryFromPID(DWORD pid)
{
    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };

    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (Process32First(hSnapshot, &pe32))
    {
        do
        {
            if (pe32.th32ProcessID == pid)
            {
                CloseHandle(hSnapshot);
                return pe32;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);

    return pe32;
}

