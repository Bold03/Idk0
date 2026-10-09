#pragma once
void CopilotInit();
void CopilotStop();
void CopilotTick(float dt);          // dipanggil tiap frame
void CopilotSay(const char* text);   // respons ala manusia
