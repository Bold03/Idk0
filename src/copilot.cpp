// Zibo 737-800 Copilot: antrean aksi dengan jeda acak (ala manusia) + logika fase terbang.
// Semua nama command/dataref diambil dari B738_Commands.txt & B738_Datarefs.txt (Zibo 3.05y).
#include "copilot.h"
#include "zibo_controls.h"
#include "XPLMUtilities.h"
#include "XPLMDataAccess.h"
#include <cmath>
#include <cstdio>
#include <deque>
#include <functional>
#include <memory>
#include <random>
#include <string>

#define Z "laminar/B738/"

// ---------------- command Zibo (terverifikasi dari file Anda) ----------------
static const char* GEAR_UP   = Z "push_button/gear_up";
static const char* GEAR_DOWN = Z "push_button/gear_down";
static const char* GEAR_OFF  = Z "push_button/gear_off";
static const char* FLAPS_UP  = "sim/flight_controls/flaps_up";    // Zibo membaca ini untuk flap handle
static const char* FLAPS_DN  = "sim/flight_controls/flaps_down";
static const char* LAND_ON   = "sim/lights/landing_lights_on";    // Zibo override: semua landing light
static const char* LAND_OFF  = "sim/lights/landing_lights_off";
static const char* FLAP_RATIO = "sim/cockpit2/controls/flap_handle_request_ratio"; // 0..1 (9 detent)

// ---------------- antrean aksi ----------------
struct Action { float delay; std::function<bool()> fn; int tries; };
static std::deque<Action> q;
static float timerQ = 0;
static bool abortFlow = false;
static bool autoMode = true;
static std::mt19937 rng{std::random_device{}()};

static float Human(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }
static void Try(float lo, float hi, int tries, std::function<bool()> f) { q.push_back({Human(lo, hi), f, tries}); }
static void Do(float lo, float hi, std::function<void()> f) { Try(lo, hi, 1, [f] { f(); return true; }); }

void CopilotSay(const char* t) {
    std::string b = std::string("[Copilot] ") + t + "\n";
    XPLMDebugString(b.c_str());
    XPLMSpeakString(t);   // TTS bawaan X-Plane, tanpa aplikasi tambahan
}
static void Say(const char* t) { std::string s = t; Do(0.3f, 0.9f, [s] { CopilotSay(s.c_str()); }); }
static void Press(const char* cmd, int n = 1) { for (int i = 0; i < n; i++) Do(0.4f, 1.0f, [cmd] { PressCmd(cmd); }); }

// Switch toggle: tekan HANYA kalau state sekarang belum sesuai (maksimal 1 kali).
static void Toggle(const char* dref, int want, const char* cmd) {
    if (!HasDataRef(dref) || !HasCmd(cmd)) {
        std::string m = std::string("[Copilot] lewati (dataref/command tidak ada): ") + cmd + "\n";
        XPLMDebugString(m.c_str()); return;
    }
    auto pressed = std::make_shared<bool>(false);
    Try(0.5f, 1.4f, 2, [=] {
        if (GetI(dref) == want) return true;
        if (!*pressed) { PressCmd(cmd); *pressed = true; return false; }
        return true;
    });
}
// Switch multi-posisi: tekan up/dn sampai dataref == want.
static void Pos(const char* dref, int want, const char* up, const char* dn) {
    if (!HasDataRef(dref)) return;
    Try(0.5f, 1.2f, 5, [=] {
        int c = GetI(dref);
        if (c == want) return true;
        PressCmd(c < want ? up : dn);
        return false;
    });
}

// ---------------- flaps ----------------
static const char* FLAP_TXT[] = {"Flaps up", "Flaps one", "Flaps two", "Flaps five", "Flaps ten",
                                 "Flaps fifteen", "Flaps twenty five", "Flaps thirty", "Flaps forty"};
static int FlapIdx() { return (int)std::lround(GetF(FLAP_RATIO) * 8.0f); }
static int DegToIdx(int deg) {
    switch (deg) { case 0: return 0; case 1: return 1; case 2: return 2; case 5: return 3; case 10: return 4;
                   case 15: return 5; case 25: return 6; case 30: return 7; case 40: return 8; }
    return 3;
}
static void FlapsTo(int idx) {
    if (!HasDataRef(FLAP_RATIO)) return;
    Try(0.6f, 1.4f, 10, [=] {
        int c = FlapIdx();
        if (c == idx) return true;
        PressCmd(c < idx ? FLAPS_DN : FLAPS_UP);
        return false;
    });
}

// ---------------- flow / prosedur ----------------
static void FlowBeforeStart() {
    Say("Before start flow.");
    Toggle(Z "fuel/fuel_tank_pos_lft1", 1, Z "toggle_switch/fuel_pump_lft1");
    Toggle(Z "fuel/fuel_tank_pos_lft2", 1, Z "toggle_switch/fuel_pump_lft2");
    Toggle(Z "fuel/fuel_tank_pos_rgt1", 1, Z "toggle_switch/fuel_pump_rgt1");
    Toggle(Z "fuel/fuel_tank_pos_rgt2", 1, Z "toggle_switch/fuel_pump_rgt2");
    Toggle(Z "toggle_switch/electric_hydro_pumps1_pos", 1, Z "toggle_switch/electric_hydro_pumps1");
    Toggle(Z "toggle_switch/electric_hydro_pumps2_pos", 1, Z "toggle_switch/electric_hydro_pumps2");
    Toggle(Z "toggle_switch/hydro_pumps1_pos", 1, Z "toggle_switch/hydro_pumps1");
    Toggle(Z "toggle_switch/hydro_pumps2_pos", 1, Z "toggle_switch/hydro_pumps2");
    Press(Z "toggle_switch/seatbelt_sign_up", 2);     // OFF -> AUTO -> ON
    Press(Z "toggle_switch/position_light_up", 2);
    Say("Before start flow complete.");
}

static void StartEngine(int n) {   // n = 1 atau 2
    const char* grd   = n == 1 ? Z "rotary/eng1_start_grd" : Z "rotary/eng2_start_grd";
    const char* off   = n == 1 ? Z "rotary/eng1_start_off" : Z "rotary/eng2_start_off";
    const char* idle  = n == 1 ? Z "engine/mixture1_idle"  : Z "engine/mixture2_idle";
    const char* nm    = n == 1 ? "Starting engine one." : "Starting engine two.";
    const char* ok    = n == 1 ? "Engine one stable." : "Engine two stable.";
    Say(nm);
    Try(0.8f, 1.8f, 1, [=] {
        if (GetF(Z "indicators/duct_press_L") < 20.0f) {      // butuh bleed air (APU / ground air)
            CopilotSay("No bleed air. Start the APU or connect ground air first.");
            abortFlow = true;
            return true;
        }
        PressCmd(grd);
        return true;
    });
    Try(1.0f, 1.5f, 60, [=] { return GetF("sim/flightmodel/engine/ENGN_N2_", n - 1) >= 25.0f; });
    Do(0.2f, 0.8f, [=] { PressCmd(idle); });                  // fuel on di ~25% N2
    Try(1.0f, 1.5f, 60, [=] { return GetI("sim/flightmodel/engine/ENGN_running", n - 1) != 0 &&
                                     GetF("sim/flightmodel/engine/ENGN_N2_", n - 1) >= 50.0f; });
    Do(0.5f, 1.5f, [=] { PressCmd(off); });                   // starter lepas
    Say(ok);
}
static void FlowStartEngines() { StartEngine(2); StartEngine(1); }

static void FlowBeforeTakeoff() {
    Say("After start flow.");
    Pos(Z "air/l_pack_pos", 1, Z "toggle_switch/l_pack_up", Z "toggle_switch/l_pack_dn");
    Pos(Z "air/r_pack_pos", 1, Z "toggle_switch/r_pack_up", Z "toggle_switch/r_pack_dn");
    Pos(Z "air/isolation_valve_pos", 1, Z "toggle_switch/iso_valve_up", Z "toggle_switch/iso_valve_dn");
    Toggle(Z "switches/autopilot/fd_ca", 1, Z "autopilot/flight_director_toggle");
    Toggle(Z "switches/autopilot/fd_fo", 1, Z "autopilot/flight_director_fo_toggle");
    Toggle(Z "switches/autopilot/at_arm", 1, Z "autopilot/autothrottle_arm_toggle");
    Press(Z "rotary/eng1_start_cont");
    Press(Z "rotary/eng2_start_cont");
    Press(Z "switch/logo_light_on");
    Do(0.5f, 1.2f, [] { PressCmd(LAND_ON); });
    int fdeg = GetI(Z "FMS/takeoff_flaps");
    int idx = DegToIdx(fdeg);
    FlapsTo(idx);
    Say(FLAP_TXT[idx]);
    Say("Before takeoff flow complete.");
}

static void FlowApproach() {
    Press(Z "toggle_switch/seatbelt_sign_up", 2);
    Do(0.5f, 1.2f, [] { PressCmd(LAND_ON); });
    Say("Approach checklist complete.");
}

static void FlowAfterLanding() {
    Say("After landing flow.");
    Do(1.0f, 2.5f, [] { PressCmd(LAND_OFF); });
    FlapsTo(0);
    Toggle(Z "switches/autopilot/fd_ca", 0, Z "autopilot/flight_director_toggle");
    Toggle(Z "switches/autopilot/fd_fo", 0, Z "autopilot/flight_director_fo_toggle");
    Toggle(Z "switches/autopilot/at_arm", 0, Z "autopilot/autothrottle_arm_toggle");
    Press(Z "switch/logo_light_off");
    Press(Z "rotary/eng1_start_off");
    Press(Z "rotary/eng2_start_off");
    Say("After landing flow complete. Welcome to the gate.");
}

// ---------------- fase penerbangan ----------------
enum class Phase { Preflight, Taxi, Takeoff, Climb, Cruise, Descent, Approach, Landing, Done };
static Phase phase = Phase::Preflight;
static bool above10k = false, gearDownDone = false, call80 = false;
static float checkTimer = -1, statusTimer = 0, ziboTimer = 0;
static bool isZibo = false;

static void SetPhase(Phase p) { phase = p; }
void CopilotOnPlaneLoaded() { checkTimer = 8.0f; phase = Phase::Preflight; above10k = gearDownDone = call80 = false; while (!q.empty()) q.pop_front(); }
void CopilotInit() { CopilotSay("Copilot ready."); }
void CopilotStop() { q.clear(); }

void CopilotMenu(int id) {
    switch (id) {
    case 1: FlowBeforeStart(); break;
    case 2: FlowStartEngines(); break;
    case 3: FlowBeforeTakeoff(); break;
    case 4: FlowApproach(); break;
    case 5: FlowAfterLanding(); break;
    case 6: q.clear(); timerQ = 0; CopilotSay("Cancelled."); break;
    case 7: autoMode = !autoMode; CopilotSay(autoMode ? "Auto mode on." : "Auto mode off."); break;
    }
}

static void SelfCheck() {
    const char* cmds[] = { GEAR_UP, GEAR_DOWN, GEAR_OFF, FLAPS_UP, FLAPS_DN, LAND_ON,
                           Z "rotary/eng1_start_grd", Z "toggle_switch/fuel_pump_lft1", Z "autopilot/cmd_a_press" };
    for (auto c : cmds) {
        char b[300]; snprintf(b, sizeof b, "[Copilot] cek command %s : %s\n", c, HasCmd(c) ? "OK" : "TIDAK ADA");
        XPLMDebugString(b);
    }
    char b[200]; snprintf(b, sizeof b, "[Copilot] cek dataref flap ratio : %s\n", HasDataRef(FLAP_RATIO) ? "OK" : "TIDAK ADA");
    XPLMDebugString(b);
}

void CopilotTick(float dt) {
    // Hanya aktif kalau pesawat = Zibo
    ziboTimer -= dt;
    if (ziboTimer <= 0) { ziboTimer = 3.0f; isZibo = HasDataRef(Z "switches/landing_gear"); }
    if (!isZibo) return;

    if (checkTimer >= 0) { checkTimer -= dt; if (checkTimer < 0) SelfCheck(); }

    float agl = GetF("sim/flightmodel/position/y_agl") * 3.28084f;
    float msl = (float)ReadNum("sim/flightmodel/position/elevation") * 3.28084f;
    float ias = GetF("sim/flightmodel/position/indicated_airspeed");
    float vvi = GetF("sim/flightmodel/position/vh_ind_fpm");
    int   onGnd = GetI("sim/flightmodel/failures/onground_any");
    int   eng1 = GetI("sim/flightmodel/engine/ENGN_running", 0);
    int   eng2 = GetI("sim/flightmodel/engine/ENGN_running", 1);

    statusTimer += dt;
    if (statusTimer > 10.0f) {
        statusTimer = 0;
        char b[260];
        snprintf(b, sizeof b, "[Copilot] fase=%d agl=%.0f msl=%.0f ias=%.0f vvi=%.0f gnd=%d eng=%d/%d flap=%d q=%d\n",
                 (int)phase, agl, msl, ias, vvi, onGnd, eng1, eng2, FlapIdx(), (int)q.size());
        XPLMDebugString(b);
    }

    // 1) Jalankan antrean aksi (satu per satu, dengan jeda acak)
    if (!q.empty()) {
        timerQ += dt;
        if (timerQ >= q.front().delay) {
            timerQ = 0;
            bool ok = q.front().fn();
            if (abortFlow) { abortFlow = false; q.clear(); return; }
            if (!ok && --q.front().tries > 0) q.front().delay = 1.0f; else q.pop_front();
        }
        return;
    }
    if (!autoMode) return;

    // 2) Logika fase
    if (!onGnd && (phase == Phase::Preflight || phase == Phase::Taxi)) SetPhase(Phase::Cruise);

    switch (phase) {
    case Phase::Preflight:
        if (onGnd && eng1 && eng2) { Say("Engines stable."); FlowBeforeTakeoff(); call80 = false; SetPhase(Phase::Taxi); }
        break;
    case Phase::Taxi:
        if (onGnd && ias >= 80 && !call80) { call80 = true; Say("Eighty knots."); SetPhase(Phase::Takeoff); }
        break;
    case Phase::Takeoff:
        if (!onGnd && vvi > 300 && agl > 15) {
            Say("Positive rate.");
            Do(0.8f, 1.8f, [] { PressCmd(GEAR_UP); });
            Say("Gear up.");
            Do(7.0f, 10.0f, [] { PressCmd(GEAR_OFF); });
            SetPhase(Phase::Climb);
        } else if (onGnd && ias < 40) SetPhase(Phase::Taxi);   // reject takeoff
        break;
    case Phase::Climb:
        if (agl > 1000 && ias >= 190 && FlapIdx() > 0) { Say("Flaps up, after takeoff checklist."); FlapsTo(0); }
        if (msl > 10000 && !above10k) { above10k = true; Say("Ten thousand."); Do(0.5f, 1.5f, [] { PressCmd(LAND_OFF); }); }
        if (msl > 10000 && FlapIdx() == 0 && std::fabs(vvi) < 400) SetPhase(Phase::Cruise);
        break;
    case Phase::Cruise:
        if (vvi < -800) { Say("Beginning descent."); SetPhase(Phase::Descent); }
        break;
    case Phase::Descent:
        if (msl < 10000 && above10k) { above10k = false; Say("Ten thousand."); Do(0.5f, 1.5f, [] { PressCmd(LAND_ON); }); }
        if (agl < 3500) { Say("Approach checklist."); FlowApproach(); gearDownDone = false; SetPhase(Phase::Approach); }
        break;
    case Phase::Approach: {
        if (agl < 2500 && !gearDownDone) { gearDownDone = true; Say("Gear down."); Do(0.8f, 1.6f, [] { PressCmd(GEAR_DOWN); }); }
        int target = -1;
        if (agl < 4000) {                       // jadwal flap berdasarkan kecepatan
            if (ias < 215) target = 1;
            if (ias < 195) target = 3;
            if (ias < 175) target = 5;
            if (ias < 155) target = 7;
        }
        if (target > FlapIdx() && gearDownDone) { Say(FLAP_TXT[target]); FlapsTo(target); }
        if (agl < 800) SetPhase(Phase::Landing);
        break;
    }
    case Phase::Landing:
        if (agl < 60 && !onGnd) { /* flare */ }
        if (onGnd && ias < 60) { FlowAfterLanding(); SetPhase(Phase::Done); }
        break;
    case Phase::Done:
        if (onGnd && ias < 5 && !eng1 && !eng2) { SetPhase(Phase::Preflight); above10k = gearDownDone = call80 = false; }
        break;
    }
}
