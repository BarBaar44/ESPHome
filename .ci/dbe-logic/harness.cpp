// Host test for the DBE control logic (DBE/DBEmain.yml, script control_fans).
//
// extract.py copies the lambda out of the YAML into body.inc. This file mocks
// just enough of ESPHome (id() globals, sensors, select, millis) to compile
// that lambda with the host compiler and run scenarios through it.
//
// It catches two things the schema check (esphome config) cannot:
//   1. C++ errors inside the lambda.
//   2. Behaviour regressions: each scenario asserts fan on/off and the reason.
//
// Run: .ci/dbe-logic/run.sh

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

using namespace std;

// ---- Minimal ESPHome mocks ----
struct Val { float state = NAN; };
struct Sel { string option = "AUTO"; string current_option() const { return option; } };
struct Txt { void publish_state(const string &) {} };

static uint32_t NOW = 0;
uint32_t millis() { return NOW; }

struct Globals {
  float fanspeed = 0;
  uint32_t temp_1_last_update = 0, temp_2_last_update = 0;
  bool heat_ok = false;
  float calc_supply = NAN, calc_return = NAN, calc_delta = NAN;
  string last_reason;
  Val temp_1, temp_2, manual_fanspeed, max_fanspeed;
  Sel fan_mode;
  Txt fan_reason;
} G;

// The lambda, wrapped so its early `return;` statements work.
static void control_fans() {
  [&]() -> void {
#include "body.inc"
  }();
}

// ---- Scenario runner ----
static int failures = 0;

static void check(bool ok, const char *name) {
  if (!ok) failures++;
  printf("%s %-50s supply %4.1f return %4.1f dT %4.1f -> fan %5.1f%%  %s\n", ok ? "PASS" : "FAIL",
         name, G.calc_supply, G.calc_return, G.calc_delta, G.fanspeed, G.last_reason.c_str());
}

// One control cycle with fresh readings. expect_on: fan running or not.
static void step(const char *name, float t1, float t2, bool expect_on, const char *expect_reason) {
  NOW += 5000;
  G.temp_1.state = t1;
  G.temp_2.state = t2;
  G.temp_1_last_update = G.temp_2_last_update = NOW;
  control_fans();
  bool on = G.fanspeed > 0;
  check(on == expect_on && G.last_reason.find(expect_reason) != string::npos, name);
}

int main() {
  G.max_fanspeed.state = 80;

  // --- Gate ---
  step("Cold pipes, boiler off", 19, 18.5, false, "not warm");
  step("Summer: pipes 29 C, no deltaT: must not start", 29, 28.8, false, "not warm");
  step("Warm water arriving 30/22: start", 30, 22, true, "below target");

  // --- Speed follows deltaT, inverted ---
  step("40/28 (dT 12): on target, minimum speed", 40, 28, true, "on target");
  float at_target = G.fanspeed;
  step("40/34 (dT 6): faster", 40, 34, true, "below target");
  float mid = G.fanspeed;
  step("40/39 (dT 1): Max Fan Speed", 40, 39, true, "below target");
  float full = G.fanspeed;
  bool order = at_target < mid && mid < full && fabsf(at_target - 25.0f) < 0.01f &&
               fabsf(full - 80.0f) < 0.01f;
  if (!order) failures++;
  printf("%s %-50s %.1f%% < %.1f%% < %.1f%% (25 at target, 80 at full)\n", order ? "PASS" : "FAIL",
         "Speed rises as deltaT falls", at_target, mid, full);

  // Sensor order does not matter: return on Dallas 1, supply on Dallas 2.
  step("Swapped sensors 34/40 gives the same speed", 34, 40, true, "below target");
  if (fabsf(G.fanspeed - mid) > 0.01f) { failures++; printf("FAIL swapped speed differs\n"); }

  // --- Residual heat after the boiler stops ---
  step("Boiler stops, cooling 30/29: keeps running", 30, 29, true, "below target");
  step("Cooling 25/24.5: still above 24, running", 25, 24.5, true, "below target");
  step("Cooling 23.8/23.5: below 24, stop", 23.8, 23.5, false, "not warm");
  step("Back to 26/25 without new start: stays off", 26, 25, false, "not warm");

  // --- Speed cap ---
  step("Warm water again 42/30: start", 42, 30, true, "on target");
  G.max_fanspeed.state = 40;
  step("Max Fan Speed 40 at dT 1", 42, 41, true, "below target");
  if (G.fanspeed > 40.01f) { failures++; printf("FAIL cap not applied (%.1f)\n", G.fanspeed); }
  G.max_fanspeed.state = 80;

  // --- Sensor faults ---
  step("DS18B20 power on reset value 85", 85, 30, false, "sensor fault");
  step("Recovery after fault needs a fresh start 42/30", 42, 30, true, "on target");
  NOW += 100000;  // no fresh readings for 100s
  control_fans();
  check(G.fanspeed == 0 && G.last_reason.find("sensor fault") != string::npos,
        "Stale readings (100s old)");

  // --- Modes ---
  G.fan_mode.option = "MANUAL";
  G.manual_fanspeed.state = 40;
  step("MANUAL 40 ignores sensors", 20, 20, true, "Manual");
  G.fan_mode.option = "OFF";
  step("OFF ignores warm water", 42, 30, false, "mode OFF");

  printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
