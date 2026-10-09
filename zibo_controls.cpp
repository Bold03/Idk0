#include "zibo_controls.h"
#include "XPLMUtilities.h"
#include "XPLMDataAccess.h"
#include <unordered_map>

static XPLMCommandRef Cmd(const std::string& n) {
    static std::unordered_map<std::string, XPLMCommandRef> cache;
    auto it = cache.find(n);
    if (it != cache.end()) return it->second;
    XPLMCommandRef r = XPLMFindCommand(n.c_str());
    if (r) cache[n] = r;
    else XPLMDebugString(("[ZiboCopilot] command tidak ditemukan: " + n + "\n").c_str());
    return r;
}
bool PressCmd(const std::string& n) { auto c = Cmd(n); if (!c) return false; XPLMCommandOnce(c); return true; }
void HoldCmdBegin(const std::string& n) { if (auto c = Cmd(n)) XPLMCommandBegin(c); }
void HoldCmdEnd(const std::string& n)   { if (auto c = Cmd(n)) XPLMCommandEnd(c); }

float GetF(const char* d, int i) {
    XPLMDataRef r = XPLMFindDataRef(d); if (!r) return 0;
    float v = 0; if (XPLMGetDatavf(r, &v, i, 1) == 1) return v;
    return XPLMGetDataf(r);
}
int GetI(const char* d, int i) {
    XPLMDataRef r = XPLMFindDataRef(d); if (!r) return 0;
    int v = 0; if (XPLMGetDatavi(r, &v, i, 1) == 1) return v;
    return XPLMGetDatai(r);
}
void SetF(const char* d, float v, int i) {
    XPLMDataRef r = XPLMFindDataRef(d); if (!r) return;
    if (XPLMGetDatavf(r, nullptr, 0, 0) > 0) XPLMSetDatavf(r, &v, i, 1);
    else XPLMSetDataf(r, v);
}
