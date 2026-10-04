// SPDX-License-Identifier: MIT
import { validateConfig } from "./SdrProtocol.js";

export const WINDOWS = ["hann", "blackman-harris"];

function windowValue(type, i, n) {
  const x = 2 * Math.PI * i / (n - 1);
  if (type === "blackman-harris") return 0.35875 - 0.48829 * Math.cos(x) + 0.14128 * Math.cos(2 * x) - 0.01168 * Math.cos(3 * x);
  return 0.5 - 0.5 * Math.cos(x);
}

// DSP receives the native ESP32-S3 IQ10 word layout. New BPRF IQ10_PACKED transport is
// expanded back to this layout by SdrClient before processing. The RF convention is
// I - jQ, as in ESPARGOS esp-web-sdr/radio.js capture(); I + jQ mirrors the RF axis.
// Apply this once, before all FFT/Welch/calibration paths, never by reversing pixels.
export class Spectrum {
  constructor(config, windowType = "hann") {
    this.config = validateConfig(config.startMHz, config.endMHz, config.sampleCount, config.sampleRate);
    this.windowType = windowType;
    const n = config.sampleCount;
    this.window = new Float32Array(n);
    this.real = new Float64Array(n);
    this.imag = new Float64Array(n);
    this.reverse = new Uint16Array(n);
    this.cos = new Float64Array(n / 2);
    this.sin = new Float64Array(n / 2);
    let sum = 0, sumSquares = 0;
    for (let i = 0; i < n; i++) {
      this.window[i] = windowValue(windowType, i, n);
      sum += this.window[i];
      sumSquares += this.window[i] * this.window[i];
      this.reverse[i] = (this.reverse[i >> 1] >> 1) | ((i & 1) ? n / 2 : 0);
    }
    this.windowSum = sum;
    this.windowSumSquares = sumSquares;
    for (let i = 0; i < n / 2; i++) {
      this.cos[i] = Math.cos(-2 * Math.PI * i / n);
      this.sin[i] = Math.sin(-2 * Math.PI * i / n);
    }
    this.normalization = sum * 512;
    const halfSpanBins = this.config.spanMHz * 1_000_000 * n / this.config.sampleRate / 2;
    this.firstBin = Math.ceil(n / 2 - halfSpanBins);
    this.endBin = Math.ceil(n / 2 + halfSpanBins);
    this.binMHz = this.config.sampleRate / n / 1_000_000;
    this.firstMHz = this.config.centerMHz + (this.firstBin - n / 2) * this.binMHz;
  }

  compute(payload, removeDc = false) {
    return this.crop(this.computeFull(payload, removeDc));
  }

  crop(full) {
    return full.slice(this.firstBin, this.endBin);
  }

  // Full fftshifted spectrum in dBFS; removeDc subtracts the mean I/Q like the Python viewer.
  computeFull(payload, removeDc = false) {
    const n = this.config.sampleCount;
    this.transform(payload, removeDc);
    const { real, imag } = this;
    const power = new Float32Array(n);
    for (let i = 0; i < n; i++) {
      const bin = (i + n / 2) % n;
      power[i] = 20 * Math.log10(Math.max(Math.hypot(real[bin], imag[bin]) / this.normalization, 1e-10));
    }
    return power;
  }

  // Leaves the unshifted complex FFT in this.real/this.imag; means overrides the per-call DC estimate.
  transform(payload, removeDc = false, means = null) {
    const n = this.config.sampleCount;
    if (payload.byteLength !== n * 4) throw new Error("IQ payload length does not match sample count.");
    const words = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
    const { real, imag, reverse, window } = this;
    let meanI = 0, meanQ = 0;
    if (means) [meanI, meanQ] = means;
    else if (removeDc) [meanI, meanQ] = Spectrum.mean(payload);
    for (let i = 0; i < n; i++) {
      const word = words.getUint32(i * 4, true);
      real[reverse[i]] = Math.fround(((((word & 1023) ^ 512) - 512) - meanI) * window[i]);
      imag[reverse[i]] = Math.fround(-(((((word >>> 10) & 1023) ^ 512) - 512) - meanQ) * window[i]);
    }
    for (let size = 2; size <= n; size *= 2) {
      const half = size / 2;
      const stride = n / size;
      for (let start = 0; start < n; start += size) {
        for (let j = 0; j < half; j++) {
          const a = start + j, b = a + half, phase = j * stride;
          const tr = this.cos[phase] * real[b] - this.sin[phase] * imag[b];
          const ti = this.sin[phase] * real[b] + this.cos[phase] * imag[b];
          real[b] = real[a] - tr;
          imag[b] = imag[a] - ti;
          real[a] += tr;
          imag[a] += ti;
        }
      }
    }
  }

  static mean(payload) {
    const count = payload.byteLength / 4;
    const words = new DataView(payload.buffer, payload.byteOffset, payload.byteLength);
    let meanI = 0, meanQ = 0;
    for (let i = 0; i < count; i++) {
      const word = words.getUint32(i * 4, true);
      meanI += ((word & 1023) ^ 512) - 512;
      meanQ += (((word >>> 10) & 1023) ^ 512) - 512;
    }
    return [meanI / count, meanQ / count];
  }
}

// Offsets (MHz) where the smoothed baseline stays within thresholdDb of its central median.
export function estimateUsableBandwidth(offsetsMHz, levelDb, thresholdDb,
    referenceMHz = 5, smoothBins = 5, ignoreMHz = 1) {
  const count = offsetsMHz.length, half = Math.floor(smoothBins / 2), smooth = new Float64Array(count);
  for (let i = 0; i < count; i++) {
    const low = Math.max(0, i - half), high = Math.min(count, i + half + 1);
    let sum = 0;
    for (let j = low; j < high; j++) sum += levelDb[j];
    smooth[i] = sum / (high - low);
  }
  const central = [];
  for (let i = 0; i < count; i++) if (Math.abs(offsetsMHz[i]) <= referenceMHz) central.push(smooth[i]);
  if (!central.length) throw new Error("No bins inside the reference region.");
  central.sort((a, b) => a - b);
  const floor = central[Math.floor(central.length / 2)] - thresholdDb;
  let centre = 0;
  for (let i = 1; i < count; i++) if (Math.abs(offsetsMHz[i]) < Math.abs(offsetsMHz[centre])) centre = i;
  // Bins next to DC are always accepted: DC removal leaves a notch there.
  const ok = i => smooth[i] >= floor || Math.abs(offsetsMHz[i]) <= ignoreMHz;
  let upper = centre, lower = centre;
  while (upper + 1 < count && ok(upper + 1)) upper++;
  while (lower > 0 && ok(lower - 1)) lower--;
  return [offsetsMHz[lower], offsetsMHz[upper]];
}
