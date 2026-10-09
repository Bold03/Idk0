#pragma once
#include <string>
// Menekan tombol/switch cockpit Zibo lewat command X-Plane.
bool PressCmd(const std::string& name);            // tekan sekali (once)
void HoldCmdBegin(const std::string& name);        // tahan
void HoldCmdEnd(const std::string& name);
float GetF(const char* dataref, int idx = 0);      // baca dataref float
int   GetI(const char* dataref, int idx = 0);      // baca dataref int
void SetF(const char* dataref, float v, int idx = 0);
