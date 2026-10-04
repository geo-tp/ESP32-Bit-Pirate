// SPDX-License-Identifier: MIT
import { reliableHalfMHz, validateConfig } from "./SdrProtocol.js";
import { Spectrum, WINDOWS, estimateUsableBandwidth } from "./Spectrum.js";

export const DIVERSITY_OFFSETS_MHZ = [-2, 0, 2];
export const MODES = ["instant", "average", "peak", "welch"];
const SPUR_BLOCK = 32, SPUR_HOT = 10 ** 0.8, FLOOR = 1e-20;
const db = value => 10 * Math.log10(Math.max(value, FLOOR));
const median = values => Float64Array.from(values).sort()[values.length >> 1];

// Use only unmasked neighbours: a strong birdie must not set its replacement level.
export function suppressMasked(power, mask, block = SPUR_BLOCK) {
  for (let start = 0; start < power.length; start += block) {
    const end = Math.min(power.length, start + block), neighbours = [];
    for (let i = start; i < end; i++) if (!mask[i]) neighbours.push(power[i]);
    if (!neighbours.length) continue;
    const middle = median(neighbours);
    for (let i = start; i < end; i++) if (mask[i]) power[i] = middle;
  }
}

function referenceMask(power, radius) {
  const mask = new Uint8Array(power.length);
  for (let start = 0; start < power.length; start += SPUR_BLOCK) {
    const end = Math.min(power.length, start + SPUR_BLOCK);
    const threshold = Math.max(FLOOR, median(power.subarray(start, end))) * SPUR_HOT;
    for (let i = start; i < end; i++) if (power[i] > threshold) {
      for (let k = Math.max(0, i - radius); k <= Math.min(power.length - 1, i + radius); k++) mask[k] = 1;
    }
  }
  return mask;
}

// Separate captures have no phase continuity. All calibration and averaging use linear power.
export class Pipeline {
  constructor(config, options = {}) {
    this.options = { window: "hann", mode: "average", alpha: 0.2, spur: true, cleanup: false, cleanupFrames: 64, diversity: false, raw: true, ...options };
    this.checkOptions(this.options);
    this.config = validateConfig(config.startMHz, config.endMHz, config.sampleCount, config.sampleRate);
    this.revision = 0;
    this.bandwidths = new Map();
    this.rebuild();
  }

  checkOptions(options) {
    if (!WINDOWS.includes(options.window) || !MODES.includes(options.mode)
        || !Number.isFinite(options.alpha) || options.alpha <= 0 || options.alpha > 1
        || !Number.isInteger(options.cleanupFrames) || options.cleanupFrames < 8 || options.cleanupFrames > 1024) {
      throw new Error("Invalid spectrum processing options.");
    }
  }

  rebuild() {
    this.n = this.config.sampleCount;
    this.spec = new Spectrum(this.config, this.options.window);
    this.welchLength = Math.min(this.n, Math.max(1024, this.n / 4));
    this.welchSpec = new Spectrum({ ...this.config, sampleCount: this.welchLength }, this.options.window);
    this.binMHz = this.config.sampleRate / this.n / 1e6;
    this.baseline = null;
    this.calibration = null;
    this.cleanupReference = this.cleanupCalibration = null;
    this.reset();
    if (this.options.cleanup) this.startCleanup();
  }

  get centers() {
    return (this.options.diversity ? DIVERSITY_OFFSETS_MHZ : [0]).map(offset => this.config.centerMHz + offset);
  }

  setConfig(config) {
    const next = validateConfig(config.startMHz, config.endMHz, config.sampleCount, config.sampleRate);
    const changed = next.sampleCount !== this.n || next.centerMHz !== this.config.centerMHz || next.sampleRate !== this.config.sampleRate;
    this.config = next;
    if (changed) { this.bandwidths.clear(); this.rebuild(); }
    else this.reset();
  }

  setOptions(next) {
    const previous = this.options, options = { ...previous, ...next };
    this.checkOptions(options);
    this.options = options;
    if (options.window !== previous.window || options.diversity !== previous.diversity) this.rebuild();
    else if (options.cleanup !== previous.cleanup) {
      if (options.cleanup) this.startCleanup();
      else { this.cleanupReference = this.cleanupCalibration = null; this.reset(); }
    }
    else if (["mode", "spur", "alpha", "cleanup"].some(key => options[key] !== previous[key])) this.reset();
  }

  reset() {
    this.gridSize = Math.ceil(this.config.spanMHz / this.binMHz);
    this.slots = new Map();
    this.dirty = new Set();
    this.lastAnalysis = null;
    this.revision++;
  }

  clearBaseline() {
    this.baseline = this.calibration = null;
    this.reset();
  }

  startCalibration(frames) {
    const collection = this.createReference(frames);
    this.clearBaseline();
    this.calibration = collection;
  }

  startCleanup() {
    this.cleanupReference = null;
    this.cleanupCalibration = this.createReference(this.options.cleanupFrames);
    this.reset();
  }

  createReference(frames) {
    if (!Number.isInteger(frames) || frames < 8 || frames > 1024) throw new Error("Use 8–1024 reference captures per center.");
    return { target: frames, entries: new Map(this.centers.map(center => [center, {
      count: 0, power: new Float64Array(this.n), welch: new Float64Array(this.welchLength)
    }])) };
  }

  accumulateReference(calibration, power, welch, center, measureEdges) {
    const entry = calibration.entries.get(center);
    if (entry.count < calibration.target) {
      for (let i = 0; i < power.length; i++) entry.power[i] += power[i];
      for (let i = 0; i < welch.length; i++) entry.welch[i] += welch[i];
      entry.count++;
    }
    if ([...calibration.entries.values()].some(item => item.count < calibration.target)) return;
    const offsets = Float64Array.from({ length: this.n }, (_, i) => (i - this.n / 2) * this.binMHz);
    const radius = this.options.window === "blackman-harris" ? 2 : 1;
    for (const item of calibration.entries.values()) {
      for (const values of [item.power, item.welch]) {
        for (let i = 0; i < values.length; i++) values[i] = Math.max(values[i] / item.count, FLOOR);
      }
      // Antenna-connected activity is not a measurement of the receiver's edges.
      const halfRateMHz = this.config.sampleRate / 2e6;
      [item.lower, item.upper] = measureEdges ? estimateUsableBandwidth(offsets, item.power.map(db), 6) : [-halfRateMHz, halfRateMHz];
      item.mask = referenceMask(item.power, radius);
      item.welchMask = referenceMask(item.welch, radius);
    }
    return calibration.entries;
  }

  fftPower(payload, removeDc) {
    const spec = this.spec, n = this.n, power = new Float64Array(n);
    spec.transform(payload, removeDc);
    for (let i = 0; i < n; i++) {
      const bin = (i + n / 2) % n;
      power[i] = (spec.real[bin] ** 2 + spec.imag[bin] ** 2) / spec.normalization ** 2;
    }
    return power;
  }

  // Two-sided Welch PSD for complex IQ, 50% overlap, per-segment DC removal.
  // Density normalization: fs * sum(window²) * fullScale² (no one-sided factor of two).
  welchPower(payload) {
    const length = this.welchLength, spec = this.welchSpec, hop = length / 2;
    const blocks = Math.floor((this.n - length) / hop) + 1;
    const sum = new Float64Array(length);
    for (let b = 0; b < blocks; b++) {
      spec.transform(payload.subarray(b * hop * 4, (b * hop + length) * 4), true);
      for (let i = 0; i < length; i++) {
        const bin = (i + length / 2) % length;
        sum[i] += spec.real[bin] ** 2 + spec.imag[bin] ** 2;
      }
    }
    const norm = blocks * this.config.sampleRate * spec.windowSumSquares * 512 ** 2;
    for (let i = 0; i < length; i++) sum[i] /= norm;
    return sum;
  }

  // Interpolate linear powers onto a fixed absolute RF grid. Missing coverage remains NaN.
  toGrid(power, center, binMHz, bounds = null) {
    const grid = new Float64Array(this.gridSize), size = power.length;
    for (let j = 0; j < grid.length; j++) {
      const offset = this.config.startMHz + j * this.binMHz - center;
      const position = offset / binMHz + size / 2;
      if (position < 0 || position > size - 1 || (bounds && (offset < bounds.lower || offset > bounds.upper))) {
        grid[j] = NaN;
        continue;
      }
      const i = Math.floor(position), t = position - i;
      grid[j] = power[i] * (1 - t) + power[Math.min(i + 1, size - 1)] * t;
    }
    return grid;
  }

  toDb(grid) {
    return Float32Array.from(grid, value => Number.isFinite(value) ? db(value) : NaN);
  }

  updateBandwidth(reading, center) {
    if (!reading) return false;
    const previous = this.bandwidths.get(center);
    this.bandwidths.set(center, reading);
    const changed = previous && ["requestedMHz", "estimatedHz", "iCode", "qCode"].some(key => previous[key] !== reading[key]);
    if (changed) {
      this.clearBaseline();
      if (this.options.cleanup) this.startCleanup();
    }
    return Boolean(changed);
  }

  process(payload, centerMHz, retuned = false, bandwidth = null) {
    if (!this.centers.includes(centerMHz)) throw new Error("Unexpected processing center frequency.");
    this.updateBandwidth(bandwidth, centerMHz);
    if (retuned) return null; // First capture after a retune is not a stable reference.
    const dcPower = this.fftPower(payload, true), rawPower = this.options.raw ? this.fftPower(payload, false) : dcPower;
    const isWelch = this.options.mode === "welch";
    const welch = isWelch || this.calibration || this.cleanupCalibration ? this.welchPower(payload) : null;
    for (const [collecting, reference, measureEdges] of [["calibration", "baseline", true], ["cleanupCalibration", "cleanupReference", false]]) {
      if (!this[collecting]) continue;
      const ready = this.accumulateReference(this[collecting], dcPower, welch, centerMHz, measureEdges);
      if (ready) { this[reference] = ready; this[collecting] = null; this.reset(); }
    }
    // One correction at a time. Keep the noise-only calibration intact for comparison.
    const quick = this.options.cleanup && this.cleanupReference?.get(centerMHz);
    const base = quick || this.baseline?.get(centerMHz);
    const source = (isWelch ? welch : dcPower).slice();
    const noise = isWelch ? base?.welch : base?.power;
    if (noise) for (let i = 0; i < source.length; i++) source[i] /= noise[i];
    // Quick cleaning deliberately uses live input: real activity in that reference
    // may also be reduced. It is opt-in, frozen after collection, and reversible.
    if (base && (quick || this.options.spur)) suppressMasked(source, isWelch ? base.welchMask : base.mask);

    let slot = this.slots.get(centerMHz);
    if (!slot) { slot = { acc: source }; this.slots.set(centerMHz, slot); }
    else if (this.options.mode === "instant" || isWelch) slot.acc.set(source);
    else for (let i = 0; i < source.length; i++) {
      if (this.options.mode === "peak") slot.acc[i] = Math.max(slot.acc[i], source[i]);
      else slot.acc[i] += this.options.alpha * (source[i] - slot.acc[i]);
    }
    this.dirty.add(centerMHz);
    const analysisFresh = this.centers.every(center => this.dirty.has(center));
    if (analysisFresh) {
      const bin = this.config.sampleRate / source.length / 1e6;
      const layers = this.centers.map(center => {
        const base = this.baseline?.get(center), reading = this.bandwidths.get(center);
        const half = reading ? reading.estimatedHz / 2e6 : this.config.spanMHz / 2;
        const bounds = { lower: Math.max(-half, base?.lower ?? -half), upper: Math.min(half, base?.upper ?? half) };
        return this.toGrid(this.slots.get(center).acc, center, bin, bounds);
      });
      this.lastAnalysis = this.toDb(this.combine(layers));
      this.dirty.clear();
    }
    return {
      raw: this.toDb(this.toGrid(rawPower, centerMHz, this.binMHz)),
      dc: this.toDb(this.toGrid(dcPower, centerMHz, this.binMHz)),
      corrected: this.lastAnalysis?.slice() ?? new Float32Array(this.gridSize).fill(NaN),
      correctedUnit: base ? "dB above reference" : isWelch ? "dBFS/Hz" : "dBFS",
      firstMHz: this.config.startMHz, binMHz: this.binMHz,
      resolutionHz: this.config.sampleRate / source.length, analysisFresh, revision: this.revision,
      status: this.status()
    };
  }

  // A full fresh tuning cycle is needed. Sequential diversity can reject short RF bursts too.
  combine(layers) {
    if (layers.length === 1) return layers[0];
    const out = new Float64Array(this.gridSize);
    for (let j = 0; j < out.length; j++) {
      const values = layers.map(layer => layer[j]);
      if (values.some(value => !Number.isFinite(value))) out[j] = NaN;
      else out[j] = this.options.spur ? median(values) : values.reduce((sum, value) => sum + value, 0) / values.length;
    }
    return out;
  }

  status() {
    const halfRateMHz = this.config.sampleRate / 2e6;
    let lower = -halfRateMHz, upper = halfRateMHz, flagged = 0;
    const offsets = [];
    if (this.baseline) for (const [center, entry] of this.baseline) {
      lower = Math.max(lower, center - this.config.centerMHz + entry.lower);
      upper = Math.min(upper, center - this.config.centerMHz + entry.upper);
      for (let i = 0; i < entry.mask.length; i++) if (entry.mask[i]) {
        flagged++;
        if (!entry.mask[i - 1] && offsets.length < 8) offsets.push(Number(((i - this.n / 2) * this.binMHz).toFixed(2)));
      }
    }
    // Edge confidence is a conservative display guide, not a measured hardware limit.
    let confidenceLower = -this.config.spanMHz * 0.4, confidenceUpper = this.config.spanMHz * 0.4;
    for (const [center, reading] of this.bandwidths) {
      const half = reliableHalfMHz(reading.estimatedHz), offset = center - this.config.centerMHz;
      confidenceLower = Math.max(confidenceLower, offset - half);
      confidenceUpper = Math.min(confidenceUpper, offset + half);
    }
    confidenceLower = Math.max(confidenceLower, lower);
    confidenceUpper = Math.min(confidenceUpper, upper);
    const entries = this.calibration ? [...this.calibration.entries.values()] : [];
    return {
      dcCorrected: true, baseline: this.calibration ? "collecting" : this.baseline ? "ready" : "none",
      cleanup: this.cleanupStatus(),
      count: entries.reduce((sum, entry) => sum + entry.count, 0),
      target: this.calibration ? this.calibration.target * this.centers.length : 0,
      confidenceLower, confidenceUpper,
      frames: this.baseline?.values().next().value?.count ?? 0, lower, upper,
      spur: { on: this.options.spur, active: this.options.spur && Boolean(this.baseline || this.options.diversity), flagged, offsets },
      diversity: this.options.diversity, centersReady: this.dirty.size, centersTotal: this.centers.length,
      mode: this.options.mode, window: this.options.window
    };
  }

  cleanupStatus() {
    const entries = this.cleanupCalibration ? [...this.cleanupCalibration.entries.values()] : [];
    return { on: this.options.cleanup, state: this.cleanupCalibration ? "collecting" : this.cleanupReference ? "ready" : "none",
      count: entries.reduce((sum, entry) => sum + entry.count, 0), target: this.options.cleanupFrames * this.centers.length,
      flagged: this.cleanupReference ? [...this.cleanupReference.values()].reduce((sum, entry) => sum + entry.mask.reduce((a, b) => a + b, 0), 0) : 0 };
  }
}
