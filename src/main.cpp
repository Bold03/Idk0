#include "copilot.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include <cstring>

#if defined(IBM)
#define XPL_API extern "C" __declspec(dllexport)
#else
#define XPL_API extern "C" __attribute__((visibility("default")))
#endif

static float FlightLoop(float dt, float, int, void*) { CopilotTick(dt); return -1.0f; } // tiap frame

XPL_API int XPluginStart(char* name, char* sig, char* desc) {
    strcpy(name, "Zibo Copilot");
    strcpy(sig,  "com.example.zibocopilot");
    strcpy(desc, "AI Copilot untuk Zibo 737-800");
    return 1;
}
XPL_API void XPluginStop(void) { CopilotStop(); }
XPL_API int  XPluginEnable(void) {
    CopilotInit();
    XPLMRegisterFlightLoopCallback(FlightLoop, -1.0f, nullptr);
    return 1;
}
XPL_API void XPluginDisable(void) { XPLMUnregisterFlightLoopCallback(FlightLoop, nullptr); }
XPL_API void XPluginReceiveMessage(XPLMPluginID, int, void*) {}
