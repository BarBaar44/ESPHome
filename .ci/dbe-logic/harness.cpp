// Host test for the DBE control logic (DBE/DBEmain.yml, script control_fans).
//
// extract.py copies the lambda out of the YAML into body.inc. This file mocks
// just enough of ESPHome (id() globals, sensors, select, API server, millis)
// to compile that lambda with the host compiler and run scenarios through it.
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
struct ApiServer { bool connected = true; bool is_connected() const { return connected; } };
namespace api { ApiServer *global_api_server; }

static uint32_t NOW = 0;
uint32_t millis() { return NOW; }

struct Globals {
  float fanspeed = 0;
  uint32_t temp_1_last_update = 0, temp_2_last_update = 0;
  bool heat_ok = false, demand_ok = false;
  float calc_flow = NAN, calc_return = NAN, calc_headroom = NAN, calc_room = NAN;
  string last_reason;
  Val temp_1, temp_2, room_temp, room_setpoint, manual_fanspeed, max_fanspeed;
  Sel fan_mode;
  Txt fan_reason;
} G;

static ApiServer API;

// The lambda, wrapped so its early `return;` statements work.
static void control_fans() {
  [&]() -> void {
#include "body.inc"
  }();
}

// ---- Scenario runner ----
static int failures = 0;

static void step(const char *name, float t1, float t2, float room, float setpoint,
                 bool expect_on, const char *expect_reason, bool ha = true) {
  NOW += 5000;
  API.connected = ha;
  G.temp_1.state = t1;
  G.temp_2.state = t2;
  G.temp_1_last_update = G.temp_2_last_update = NOW;
  G.room_temp.state = room;
  G.room_setpoint.state = setpoint;

  control_fans();

  bool on = G.fanspeed > 0;
  bool ok = on == expect_on && G.last_reason.find(expect_reason) != string::npos;
  if (!ok) failures++;
  printf("%s %-44s head %5.1f -> fan %5.1f%%  %s\n", ok ? "PASS" : "FAIL", name,
         G.calc_headroom, G.fanspeed, G.last_reason.c_str());
}

int main() {
  api::global_api_server = &API;
  G.max_fanspeed.state = 80;

  // Heat gate
  step("Boiler off, pipes near room temp", 20, 19.5, 19, 20, false, "no heat");
  step("Boiler fires, radiator hot, room cold", 55, 48, 18, 20, true, "heating room");
  step("Low temperature water 35/30", 35, 30, 19, 20, true, "heating room");
  step("Cooling 30/27: still above off threshold", 30, 27, 19, 20, true, "heating room");
  step("Cooling 25/23: below off threshold", 25, 23, 19, 20, false, "no heat");
  step("Warming 32/28: headroom 11, back on", 32, 28, 19, 20, true, "heating room");

  // Old logic's failure case: large flow/return gap from a throttled valve.
  // New logic: driven by headroom and demand, not by the gap.
  step("Throttled TRV 50/30, room still below target", 50, 30, 19.8, 20, true, "heating room");
  step("Throttled TRV 50/30, room above target", 50, 30, 20.3, 20, false, "room at setpoint");

  // Demand gate with hysteresis
  step("Room back to 19.95: stays off (hysteresis)", 55, 48, 19.95, 20, false, "room at setpoint");
  step("Room 19.8: back on", 55, 48, 19.8, 20, true, "heating room");
  step("Room climate off in HA (no setpoint)", 55, 48, 18, NAN, false, "heating off in HA");

  // Standalone fallback
  step("HA disconnected, hot radiator", 55, 48, 18, 20, true, "standalone", false);
  step("HA disconnected, cold radiator", 21, 20, 18, 20, false, "no heat", false);

  // Speed cap
  G.max_fanspeed.state = 40;
  step("Max Fan Speed 40", 70, 65, 18, 20, true, "heating room");
  if (G.fanspeed > 40.01f) { failures++; printf("FAIL cap not applied (%.1f)\n", G.fanspeed); }
  G.max_fanspeed.state = 80;

  // Sensor faults
  step("DS18B20 power on reset value 85", 85, 48, 18, 20, false, "sensor fault");
  NOW += 100000;  // no fresh readings for 100s
  G.temp_1.state = 55; G.temp_2.state = 48;
  control_fans();
  bool stale_ok = G.fanspeed == 0 && G.last_reason.find("sensor fault") != string::npos;
  if (!stale_ok) failures++;
  printf("%s %-44s -> fan %5.1f%%  %s\n", stale_ok ? "PASS" : "FAIL", "Stale readings (100s old)",
         G.fanspeed, G.last_reason.c_str());

  // Modes
  G.fan_mode.option = "MANUAL";
  G.manual_fanspeed.state = 40;
  step("MANUAL 40 ignores sensors", 20, 20, 20, 20, true, "Manual");
  G.fan_mode.option = "OFF";
  step("OFF ignores demand", 55, 48, 18, 20, false, "mode OFF");

  printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
