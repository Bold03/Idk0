#pragma once
#include <string>
// Menekan tombol / baca-tulis switch Zibo lewat command & dataref X-Plane.
bool   HasCmd(const std::string& name);
bool   HasDataRef(const char* name);
bool   PressCmd(const std::string& name);     // tekan sekali
double ReadNum(const char* dref, int idx = 0); // baca int/float/double/array apa pun
float  GetF(const char* dref, int idx = 0);
int    GetI(const char* dref, int idx = 0);
void   WriteNum(const char* dref, double v, int idx = 0);
