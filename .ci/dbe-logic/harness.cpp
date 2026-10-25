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
  Sel speed_curve;
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
  G.speed_curve.option = "Rising";

  // --- Gate ---
  step("Cold pipes, boiler off", 19, 18.5, false, "not warm");
  step("Summer: pipes 27 C, no deltaT: must not start", 27, 26.8, false, "not warm");
  step("Sensor offset on cold pipe 21/19.5: must not start", 21, 19.5, false, "not warm");
  step("Low supply 26/22 (mild day): start", 26, 22, true, "rising curve");

  // --- Rising curve (default, original behaviour): faster as deltaT grows ---
  step("Rising, dT 2: minimum speed", 40, 38, true, "rising curve");
  float r_lo = G.fanspeed;
  step("Rising, dT 11: faster", 40, 29, true, "rising curve");
  float r_mid = G.fanspeed;
  step("Rising, dT 20: Max Fan Speed", 42, 22, true, "rising curve");
  float r_hi = G.fanspeed;
  bool r_ok = r_lo < r_mid && r_mid < r_hi && fabsf(r_lo - 25.0f) < 0.01f && fabsf(r_hi - 80.0f) < 0.01f;
  if (!r_ok) failures++;
  printf("%s %-50s %.1f%% < %.1f%% < %.1f%%\n", r_ok ? "PASS" : "FAIL", "Rising: speed grows with deltaT", r_lo, r_mid, r_hi);

  // --- Falling curve: faster as deltaT shrinks ---
  G.speed_curve.option = "Falling";
  step("Falling, dT 12: minimum speed", 40, 28, true, "falling curve");
  float f_lo = G.fanspeed;
  step("Falling, dT 6: faster", 40, 34, true, "falling curve");
  float f_mid = G.fanspeed;
  step("Falling, dT 1: Max Fan Speed", 40, 39, true, "falling curve");
  float f_hi = G.fanspeed;
  bool f_ok = f_lo < f_mid && f_mid < f_hi && fabsf(f_lo - 25.0f) < 0.01f && fabsf(f_hi - 80.0f) < 0.01f;
  if (!f_ok) failures++;
  printf("%s %-50s %.1f%% < %.1f%% < %.1f%%\n", f_ok ? "PASS" : "FAIL", "Falling: speed grows as deltaT shrinks", f_lo, f_mid, f_hi);

  // Sensor order does not matter: return on Dallas 1, supply on Dallas 2.
  step("Swapped sensors 34/40 gives the same speed", 34, 40, true, "falling curve");
  if (fabsf(G.fanspeed - f_mid) > 0.01f) { failures++; printf("FAIL swapped speed differs\n"); }

  // --- Residual heat after the boiler stops (both curves keep running) ---
  step("Falling: boiler stops, cooling 30/29: running", 30, 29, true, "falling curve");
  G.speed_curve.option = "Rising";
  step("Rising: cooling 28/27.5: running at minimum", 28, 27.5, true, "rising curve");
  if (fabsf(G.fanspeed - 25.0f) > 0.01f) { failures++; printf("FAIL rising residual not at minimum\n"); }
  step("Cooling 24.5/24.2: still above 24, running", 24.5, 24.2, true, "rising curve");
  step("Cooling 23.8/23.5: below 24, stop", 23.8, 23.5, false, "not warm");
  step("Back to 26/25.5 without water moving: stays off", 26, 25.5, false, "not warm");

  // --- Speed cap ---
  step("Warm water again 42/22: start", 42, 22, true, "rising curve");
  G.max_fanspeed.state = 40;
  step("Max Fan Speed 40 at dT 20", 42, 22, true, "rising curve");
  if (G.fanspeed > 40.01f) { failures++; printf("FAIL cap not applied (%.1f)\n", G.fanspeed); }
  G.max_fanspeed.state = 80;

  // --- Sensor faults ---
  step("DS18B20 power on reset value 85", 85, 30, false, "sensor fault");
  step("Recovery after fault needs a fresh start 42/30", 42, 30, true, "rising curve");
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
