#include "zibo_controls.h"
#include "XPLMUtilities.h"
#include "XPLMDataAccess.h"
#include <cmath>
#include <unordered_map>

static XPLMCommandRef Cmd(const std::string& n) {
    static std::unordered_map<std::string, XPLMCommandRef> cache;
    auto it = cache.find(n);
    if (it != cache.end()) return it->second;
    XPLMCommandRef r = XPLMFindCommand(n.c_str());
    if (r) cache[n] = r;
    return r;
}
bool HasCmd(const std::string& n) { return Cmd(n) != nullptr; }
bool HasDataRef(const char* d) { return XPLMFindDataRef(d) != nullptr; }

bool PressCmd(const std::string& n) {
    auto c = Cmd(n);
    if (!c) { XPLMDebugString(("[ZiboCopilot] command TIDAK ADA: " + n + "\n").c_str()); return false; }
    XPLMCommandOnce(c);
    return true;
}

double ReadNum(const char* d, int i) {
    XPLMDataRef r = XPLMFindDataRef(d);
    if (!r) return 0;
    XPLMDataTypeID t = XPLMGetDataRefTypes(r);
    if (t & xplmType_FloatArray) { float v = 0; XPLMGetDatavf(r, &v, i, 1); return v; }
    if (t & xplmType_IntArray)   { int v = 0;   XPLMGetDatavi(r, &v, i, 1); return v; }
    if (t & xplmType_Double) return XPLMGetDatad(r);
    if (t & xplmType_Float)  return XPLMGetDataf(r);
    if (t & xplmType_Int)    return XPLMGetDatai(r);
    return 0;
}
float GetF(const char* d, int i) { return (float)ReadNum(d, i); }
int   GetI(const char* d, int i) { return (int)std::lround(ReadNum(d, i)); }

void WriteNum(const char* d, double v, int i) {
    XPLMDataRef r = XPLMFindDataRef(d);
    if (!r) return;
    XPLMDataTypeID t = XPLMGetDataRefTypes(r);
    if (t & xplmType_FloatArray) { float x = (float)v; XPLMSetDatavf(r, &x, i, 1); return; }
    if (t & xplmType_IntArray)   { int x = (int)std::lround(v); XPLMSetDatavi(r, &x, i, 1); return; }
    if (t & xplmType_Double) { XPLMSetDatad(r, v); return; }
    if (t & xplmType_Float)  { XPLMSetDataf(r, (float)v); return; }
    if (t & xplmType_Int)    { XPLMSetDatai(r, (int)std::lround(v)); return; }
}
