// "Otak" copilot: state machine berbasis fase penerbangan + delay acak ala manusia.
#include "copilot.h"
#include "zibo_controls.h"
#include "XPLMUtilities.h"
#include "XPLMDataAccess.h"
#include <random>
#include <queue>
#include <string>
#include <functional>
#include <cstdio>

enum class Phase { Preflight, Taxi, Takeoff, Climb, Cruise, Descent, Approach, Landing, Done };
static Phase phase = Phase::Preflight;

struct Action { float delay; std::function<void()> fn; };
static std::queue<Action> q;
static float waitTimer = 0;
static std::mt19937 rng{std::random_device{}()};

static float Human(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }
static void Do(float lo, float hi, std::function<void()> f) { q.push({Human(lo, hi), f}); }

void CopilotSay(const char* t) {
    char b[256]; snprintf(b, sizeof b, "[Copilot] %s\n", t);
    XPLMDebugString(b);
    XPLMSpeakString(t);   // TTS bawaan X-Plane, tanpa aplikasi tambahan
}

// ------------------------------------------------------------------
// ISI NAMA COMMAND ZIBO DI SINI (cek lewat DataRefTool / command list Zibo).
// Contoh format: "laminar/B738/toggle_switch/..."
// ------------------------------------------------------------------
static const char* CMD_GEAR_UP   = "laminar/B738/push_button/gear_up";    // VERIFIKASI
static const char* CMD_GEAR_DOWN = "laminar/B738/push_button/gear_down";  // VERIFIKASI
static const char* CMD_FLAPS_UP  = "laminar/B738/push_button/flap_up";    // VERIFIKASI
static const char* CMD_FLAPS_DN  = "laminar/B738/push_button/flap_down";  // VERIFIKASI

static void SetPhase(Phase p) { phase = p; }

void CopilotInit() { CopilotSay("Copilot siap."); }
void CopilotStop() { while (!q.empty()) q.pop(); }

void CopilotTick(float dt) {
    // 1) Jalankan antrean aksi dengan jeda acak (terasa seperti manusia)
    if (!q.empty()) {
        waitTimer += dt;
        if (waitTimer >= q.front().delay) { q.front().fn(); q.pop(); waitTimer = 0; }
        return;
    }

    // 2) Logika: baca kondisi pesawat lalu putuskan
    float agl   = GetF("sim/flightmodel/position/y_agl") * 3.28084f;  // ft
    float ias   = GetF("sim/flightmodel/position/indicated_airspeed"); // knots
    float vvi   = GetF("sim/flightmodel/position/vh_ind_fpm");
    int   onGnd = GetI("sim/flightmodel/failures/onground_any");

    switch (phase) {
    case Phase::Preflight:
        if (onGnd && GetF("sim/flightmodel/engine/ENGN_running", 0) > 0) {
            Do(1, 3, [] { CopilotSay("Engine stabil. Siap taxi."); SetPhase(Phase::Taxi); });
        }
        break;
    case Phase::Taxi:
        if (ias > 60) { Do(0.5f, 1.2f, [] { CopilotSay("Delapan puluh knot."); SetPhase(Phase::Takeoff); }); }
        break;
    case Phase::Takeoff:
        if (!onGnd && vvi > 300) {
            Do(0.6f, 1.5f, [] { CopilotSay("Positive rate."); });
            Do(0.8f, 1.8f, [] { PressCmd(CMD_GEAR_UP); CopilotSay("Gear up."); SetPhase(Phase::Climb); });
        }
        break;
    case Phase::Climb:
        if (agl > 3000 && ias > 210) { Do(1, 2, [] { PressCmd(CMD_FLAPS_UP); CopilotSay("Flaps up."); }); SetPhase(Phase::Cruise); }
        break;
    case Phase::Cruise:
        if (vvi < -500) { Do(1, 2, [] { CopilotSay("Mulai descent."); SetPhase(Phase::Descent); }); }
        break;
    case Phase::Descent:
        if (agl < 3000) { Do(1, 2, [] { CopilotSay("Tiga ribu kaki, approach checklist."); SetPhase(Phase::Approach); }); }
        break;
    case Phase::Approach:
        if (agl < 2000 && ias < 200) { Do(1, 2, [] { PressCmd(CMD_GEAR_DOWN); CopilotSay("Gear down, flaps lima."); SetPhase(Phase::Landing); }); }
        break;
    case Phase::Landing:
        if (agl < 50) { Do(0.2f, 0.6f, [] { CopilotSay("Lima puluh."); }); }
        if (onGnd && ias < 60) { Do(1, 2, [] { CopilotSay("Taxi in, selamat datang."); SetPhase(Phase::Done); }); }
        break;
    default: break;
    }
}
