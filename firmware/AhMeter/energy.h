// energy.h - hardware-independent math for the Ah/Wh meter.
// No Arduino headers here on purpose: this file is compiled on the host
// by test/test_energy.cpp so the integration math can be unit tested.
#pragma once
#include <stdint.h>

namespace ahm {

// ---------------------------------------------------------------------------
// Current sensor conversion (Allegro ACS71x family, ratiometric output)
// ---------------------------------------------------------------------------
// The ACS71x output is ratiometric: both the zero-current offset and the
// sensitivity scale with Vcc. If we work in ADC counts (also ratiometric to
// Vcc when using the DEFAULT reference) the Vcc term cancels out, so current
// does NOT need the Vcc correction that the voltage channel needs.
//
//   amps = (counts - zeroCounts) * (5000 mV / 1024) / sens_mV_per_A
//
// sumCounts / zeroSum are oversampled sums (OVERSAMPLE readings added).
inline float countsToAmps(int32_t sumCounts, int32_t zeroSum,
                          uint8_t oversample, float sensMvPerAmp) {
  const float counts = (float)(sumCounts - zeroSum) / (float)oversample;
  return counts * (5000.0f / 1024.0f) / sensMvPerAmp;
}

// ---------------------------------------------------------------------------
// Voltage channel
// ---------------------------------------------------------------------------
// vccMv  : measured supply (via the internal 1.1 V bandgap), not assumed 5.000
// ratio  : (R1 + R2) / R2
// scale  : user calibration factor (1.0 = uncalibrated), set with the "v" cmd
inline float countsToVolts(int32_t sumCounts, uint8_t oversample,
                           float vccMv, float ratio, float scale) {
  const float pinMv = ((float)sumCounts / (float)oversample) * vccMv / 1024.0f;
  return pinMv * ratio * scale / 1000.0f;
}

// ---------------------------------------------------------------------------
// Coulomb / energy integrator
// ---------------------------------------------------------------------------
// Integrates I*dt and V*I*dt on every sample, using the real elapsed time of
// each interval. Accumulators are 64-bit integers in mA*ms and mW*ms, so there
// is no float precision loss and no practical overflow:
//   int64 max / (30 A * 30 V in mW) ~ 1e10 s ~ 300+ years.
// Sign convention: POSITIVE current = discharge (load drawing from battery),
//               NEGATIVE current = charge (source pushing into battery).
//   Flip SENSOR_INVERT in config.h if your sensor is mounted the other way.
struct Totals {
  int64_t mAms_out = 0;   // discharge charge,  mA*ms
  int64_t mAms_in  = 0;   // charge charge,     mA*ms
  int64_t mWms_out = 0;   // discharge energy,  mW*ms
  int64_t mWms_in  = 0;   // charge energy,     mW*ms
  uint32_t seconds = 0;   // integrated run time, whole seconds
  uint32_t msRemainder = 0;

  void reset() { *this = Totals(); }

  // dtMs is the measured interval since the previous sample.
  void add(float volts, float amps, uint32_t dtMs) {
    const int32_t mA = (int32_t)(amps * 1000.0f + (amps >= 0 ? 0.5f : -0.5f));
    const int32_t mW = (int32_t)(volts * amps * 1000.0f + (amps >= 0 ? 0.5f : -0.5f));
    if (mA >= 0) {
      mAms_out += (int64_t)mA * dtMs;
      mWms_out += (int64_t)mW * dtMs;
    } else {
      mAms_in  += (int64_t)(-mA) * dtMs;
      mWms_in  += (int64_t)(-mW) * dtMs;
    }
    msRemainder += dtMs;
    seconds += msRemainder / 1000;
    msRemainder %= 1000;
  }

  static constexpr float MAMS_PER_AH = 3.6e9f;  // 1000 mA * 3600 s * 1000 ms
  float ahOut() const { return (float)mAms_out / MAMS_PER_AH; }
  float ahIn()  const { return (float)mAms_in  / MAMS_PER_AH; }
  float ahNet() const { return (float)(mAms_out - mAms_in) / MAMS_PER_AH; }
  float whOut() const { return (float)mWms_out / MAMS_PER_AH; }
  float whIn()  const { return (float)mWms_in  / MAMS_PER_AH; }
  float whNet() const { return (float)(mWms_out - mWms_in) / MAMS_PER_AH; }
};

// Battery "gas gauge": capacity minus net discharge, with a charge
// efficiency factor (lead-acid ~0.85, LiFePO4 ~0.99).
inline float stateOfChargePct(const Totals& t, float capacityAh,
                              float startSocPct, float chargeEff) {
  if (capacityAh <= 0) return -1.0f;
  const float usedAh = t.ahOut() - t.ahIn() * chargeEff;
  float soc = startSocPct - usedAh / capacityAh * 100.0f;
  if (soc > 100.0f) soc = 100.0f;
  if (soc < 0.0f) soc = 0.0f;
  return soc;
}

// ---------------------------------------------------------------------------
// Low voltage disconnect with hysteresis and a hold-off delay
// ---------------------------------------------------------------------------
// Load is disconnected only after the voltage stays below offV for delayMs
// (so motor-start sag doesn't trip it), and reconnected once it rises above
// onV for delayMs.
struct Lvd {
  float offV, onV;
  uint32_t delayMs;
  bool loadOn = true;
  uint32_t timerMs = 0;

  Lvd(float off, float on, uint32_t delay) : offV(off), onV(on), delayMs(delay) {}

  // Returns the new load state.
  bool update(float volts, uint32_t dtMs) {
    const bool wantChange = loadOn ? (volts < offV) : (volts > onV);
    if (wantChange) {
      timerMs += dtMs;
      if (timerMs >= delayMs) { loadOn = !loadOn; timerMs = 0; }
    } else {
      timerMs = 0;
    }
    return loadOn;
  }
};

}  // namespace ahm
