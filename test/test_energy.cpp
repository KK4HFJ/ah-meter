// Host-side unit tests for firmware/AhMeter/energy.h
// Build & run:  make test
#include <cmath>
#include <cstdio>
#include "../firmware/AhMeter/energy.h"

using namespace ahm;

static int failures = 0;
#define NEAR(a, b, tol) check(#a, (a), (b), (tol), __LINE__)
static void check(const char* what, double got, double want, double tol, int line) {
  if (std::fabs(got - want) > tol) {
    std::printf("FAIL line %d: %s = %.6f, expected %.6f\n", line, what, got, want);
    failures++;
  }
}

int main() {
  // --- current conversion: ACS715, 500 mV zero, 133 mV/A, 16x oversample
  const int32_t zero = (int32_t)(0.5 / 5.0 * 1024 * 16);           // 1638
  // 10 A -> 500 + 1330 = 1830 mV -> 374.78 counts
  NEAR(countsToAmps((int32_t)(1.830 / 5.0 * 1024 * 16), zero, 16, 133.0f), 10.0, 0.02);
  // 33 A would have overflowed the old int outputValue; must be fine here
  NEAR(countsToAmps((int32_t)((0.5 + 33 * 0.133) / 5.0 * 1024 * 16), zero, 16, 133.0f), 33.0, 0.05);
  // bidirectional ACS714 (2500 mV zero, 66 mV/A), -5 A
  const int32_t z714 = 2.5 / 5.0 * 1024 * 16;
  NEAR(countsToAmps((int32_t)((2.5 - 0.33) / 5.0 * 1024 * 16), z714, 16, 66.0f), -5.0, 0.05);

  // --- voltage: 47k/10k divider, Vcc actually 4.80 V (USB sag)
  const float ratio = 57.0f / 10.0f;
  const float vbat = 13.2f;
  const float pinV = vbat / ratio;
  const int32_t vsum = (int32_t)(pinV / 4.80f * 1024 * 16 + 0.5f);
  NEAR(countsToVolts(vsum, 16, 4800.0f, ratio, 1.0f), 13.2, 0.01);
  // The 2012 method (assume 5.000 V) would read ~4% high:
  NEAR(countsToVolts(vsum, 16, 5000.0f, ratio, 1.0f), 13.75, 0.02);

  // --- integration: 10 A at 12.5 V for 1 hour in 100 ms steps -> 10 Ah, 125 Wh
  {
    Totals t;
    for (int i = 0; i < 36000; i++) t.add(12.5f, 10.0f, 100);
    NEAR(t.ahOut(), 10.0, 1e-4);
    NEAR(t.whOut(), 125.0, 1e-3);
    NEAR(t.seconds, 3600, 0);
  }

  // --- uneven dt must still integrate correctly (old code assumed uniform loops)
  {
    Totals t;
    t.add(12.0f, 5.0f, 3600000);   // 5 A for 1 h in one step
    t.add(12.0f, 1.0f, 100);       // 1 A for 100 ms
    NEAR(t.ahOut(), 5.0 + 1.0 * 0.1 / 3600, 1e-5);
  }

  // --- Wh must use V at each moment, not final V x total Ah (old bug)
  {
    Totals t;
    t.add(14.0f, 10.0f, 1800000);  // 30 min at 14 V
    t.add(11.0f, 10.0f, 1800000);  // 30 min at 11 V
    NEAR(t.whOut(), 125.0, 1e-3);  // old code would report 11 V * 10 Ah = 110
  }

  // --- long run: 30 days at 2.5 A, no overflow, no drift (old int sample overflowed in ~1 h)
  {
    Totals t;
    const long steps = 30L * 24 * 3600 * 10;
    for (long i = 0; i < steps; i++) t.add(12.8f, 2.5f, 100);
    NEAR(t.ahOut(), 2.5 * 24 * 30, 0.01);
    NEAR(t.seconds, 30.0 * 24 * 3600, 0);
  }

  // --- charge in / out split and SOC
  {
    Totals t;
    t.add(12.5f, 20.0f, 3600000);   // 20 Ah out
    t.add(14.2f, -10.0f, 3600000);  // 10 Ah in
    NEAR(t.ahOut(), 20.0, 1e-4);
    NEAR(t.ahIn(), 10.0, 1e-4);
    NEAR(t.ahNet(), 10.0, 1e-4);
    NEAR(t.whIn(), 142.0, 1e-3);
    NEAR(stateOfChargePct(t, 100.0f, 100.0f, 1.0f), 90.0, 1e-3);
    NEAR(stateOfChargePct(t, 100.0f, 100.0f, 0.85f), 88.5, 1e-3);
    NEAR(stateOfChargePct(t, 0.0f, 100.0f, 1.0f), -1.0, 0);   // disabled
  }

  // --- LVD: hysteresis + hold-off delay
  {
    Lvd l(11.5f, 12.6f, 10000);
    // brief sag (motor start) must not trip
    for (int i = 0; i < 50; i++) l.update(10.9f, 100);   // 5 s
    if (!l.loadOn) { std::puts("FAIL: LVD tripped on short sag"); failures++; }
    l.update(12.2f, 100);                                // recovers, timer resets
    for (int i = 0; i < 99; i++) l.update(11.3f, 100);   // 9.9 s
    if (!l.loadOn) { std::puts("FAIL: LVD tripped early"); failures++; }
    l.update(11.3f, 100);                                // 10.0 s
    if (l.loadOn) { std::puts("FAIL: LVD did not trip"); failures++; }
    // between thresholds: stays off
    for (int i = 0; i < 200; i++) l.update(12.3f, 100);
    if (l.loadOn) { std::puts("FAIL: LVD reconnected inside hysteresis band"); failures++; }
    for (int i = 0; i < 100; i++) l.update(12.8f, 100);
    if (!l.loadOn) { std::puts("FAIL: LVD did not reconnect"); failures++; }
  }

  if (failures) { std::printf("%d test(s) failed\n", failures); return 1; }
  std::puts("All tests passed");
  return 0;
}
