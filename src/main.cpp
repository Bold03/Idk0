#include "copilot.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMMenus.h"
#include <cstring>
#include <cstdint>

#if defined(IBM)
#define XPL_API extern "C" __declspec(dllexport)
#else
#define XPL_API extern "C" __attribute__((visibility("default")))
#endif

static XPLMMenuID gMenu = nullptr;
static void MenuCB(void*, void* item) { CopilotMenu((int)(intptr_t)item); }

static float FlightLoop(float dt, float, int, void*) { CopilotTick(dt); return -1.0f; } // tiap frame

XPL_API int XPluginStart(char* name, char* sig, char* desc) {
    strcpy(name, "Zibo Copilot");
    strcpy(sig,  "com.example.zibocopilot");
    strcpy(desc, "AI Copilot untuk Zibo 737-800");
    int idx = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Zibo Copilot", nullptr, 1);
    gMenu = XPLMCreateMenu("Zibo Copilot", XPLMFindPluginsMenu(), idx, MenuCB, nullptr);
    XPLMAppendMenuItem(gMenu, "1. Before Start flow",   (void*)1, 1);
    XPLMAppendMenuItem(gMenu, "2. Start engines (2 then 1)", (void*)2, 1);
    XPLMAppendMenuItem(gMenu, "3. After Start / Before Takeoff flow", (void*)3, 1);
    XPLMAppendMenuItem(gMenu, "4. Approach flow",       (void*)4, 1);
    XPLMAppendMenuItem(gMenu, "5. After Landing flow",  (void*)5, 1);
    XPLMAppendMenuItem(gMenu, "Cancel current actions", (void*)6, 1);
    XPLMAppendMenuItem(gMenu, "Toggle auto mode",       (void*)7, 1);
    return 1;
}
XPL_API void XPluginStop(void) { CopilotStop(); if (gMenu) XPLMDestroyMenu(gMenu); }
XPL_API int  XPluginEnable(void) {
    CopilotInit();
    XPLMRegisterFlightLoopCallback(FlightLoop, -1.0f, nullptr);
    return 1;
}
XPL_API void XPluginDisable(void) { XPLMUnregisterFlightLoopCallback(FlightLoop, nullptr); }
XPL_API void XPluginReceiveMessage(XPLMPluginID, int msg, void* param) {
    if (msg == XPLM_MSG_PLANE_LOADED && (intptr_t)param == 0) CopilotOnPlaneLoaded();
}
