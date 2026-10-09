#pragma once
void CopilotInit();
void CopilotStop();
void CopilotTick(float dt);           // dipanggil tiap frame
void CopilotSay(const char* text);    // bicara (TTS bawaan X-Plane)
void CopilotOnPlaneLoaded();
void CopilotMenu(int id);             // dari menu Plugins > Zibo Copilot
