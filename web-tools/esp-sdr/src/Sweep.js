// SPDX-License-Identifier: MIT
import { Pipeline } from "./Pipeline.js";
import { SpectrumSmoother } from "./SpectrumSmoother.js";
import { SAMPLE_COUNTS, DEFAULT_SAMPLE_RATE, bandwidthMHz, reliableHalfMHz, validateConfig } from "./SdrProtocol.js";

export const SCAN_PROFILES = {
  fast: { sampleCount: 256, dwellFrames: 1, overlap: 0.12, binMHz: 1, threshold: 12 },
  balanced: { sampleCount: 512, dwellFrames: 1, overlap: 0.12, binMHz: 0.5, threshold: 8 },
  sensitive: { sampleCount: 1024, dwellFrames: 2, overlap: 0.2, binMHz: 0.25, threshold: 6 }
};

const db = power => 10 * Math.log10(Math.max(1e-20, power));

export function planSweep(startMHz, endMHz, profile = "fast", info = {}, bandwidth = null) {
  const min = Number(info.center_min_mhz ?? 100), max = Number(info.center_max_mhz ?? 6000);
  if (!Number.isFinite(startMHz) || !Number.isFinite(endMHz) || startMHz < min || endMHz > max || endMHz - startMHz < 2) {
    throw new Error(`Choose a scan range within ${min}–${max} MHz, at least 2 MHz wide.`);
  }
  if (!Object.hasOwn(SCAN_PROFILES, profile)) throw new Error("Choose Fast, Balanced or Sensitive.");
  const options = SCAN_PROFILES[profile];
  const sampleCount = SAMPLE_COUNTS.filter(n => n <= Math.min(options.sampleCount, Number(info.max_samples ?? 16380))).at(-1);
  if (!sampleCount) throw new Error("The adapter cannot capture enough samples.");
  const sampleRate = Number(info.fs_hz ?? DEFAULT_SAMPLE_RATE);
  const rxBandwidthHz = bandwidthMHz(bandwidth) * 1e6;
  const sliceHalfMHz = Math.min(reliableHalfMHz(rxBandwidthHz), sampleRate / 1e6 * 0.4);
  // Crop uncertain analog edges, then overlap central slices. Integer centers
  // round INWARD at the range endpoints, so fractional band edges remain covered.
  const stepMHz = Math.max(1, Math.floor(2 * sliceHalfMHz * (1 - options.overlap)));
  const clamp = value => Math.max(min, Math.min(max, value));
  const first = clamp(Math.floor(Math.min(startMHz + sliceHalfMHz, (startMHz + endMHz) / 2)));
  const last = clamp(Math.ceil(Math.max(endMHz - sliceHalfMHz, first)));
  const steps = Math.ceil((last - first) / stepMHz);
  const centers = steps ? Array.from({ length: steps + 1 }, (_, i) => Math.round(first + i * (last - first) / steps)) : [first];
  return { startMHz, endMHz, spanMHz: endMHz - startMHz, centers, profile, sampleCount, rxBandwidthHz, sliceHalfMHz, stepMHz,
    dwellFrames: options.dwellFrames, threshold: options.threshold, binMHz: options.binMHz,
    fftBinMHz: sampleRate / sampleCount / 1e6, sampleRate };
}

// Estimate the nearby median noise floor. Occupancy is sampled on visits, not continuous airtime.
export function localNoise(levels, binMHz) {
  const noise = new Float32Array(levels.length).fill(NaN);
  const block = Math.max(8, Math.round(1 / binMHz)), radius = Math.max(block, Math.round(5 / binMHz));
  for (let i = 0; i < levels.length; i += block) {
    const neighbours = Array.from(levels.subarray(Math.max(0, i - radius), Math.min(levels.length, i + block + radius)))
      .filter(Number.isFinite).sort((a, b) => a - b);
    if (neighbours.length) noise.fill(neighbours[neighbours.length >> 1], i, Math.min(levels.length, i + block));
  }
  return noise;
}

export function signalRegions(levels, noise, firstMHz, binMHz, threshold = 8) {
  const regions = [], gapBins = Math.max(1, Math.round(0.15 / binMHz));
  let region = null;
  for (let i = 0; i < levels.length; i++) {
    if (!Number.isFinite(levels[i]) || levels[i] < noise[i] + threshold) continue;
    if (!region || i - region.last > gapBins) {
      region = { first: i, last: i, peak: i, level: levels[i], excess: levels[i] - noise[i] };
      regions.push(region);
    } else {
      region.last = i;
      if (levels[i] > region.level) { region.peak = i; region.level = levels[i]; region.excess = levels[i] - noise[i]; }
    }
  }
  return regions.filter(r => r.last > r.first || r.excess >= threshold + 2)
    .sort((a, b) => b.excess - a.excess).slice(0, 12).map(r => ({
      startMHz: firstMHz + (r.first - 0.5) * binMHz, endMHz: firstMHz + (r.last + 0.5) * binMHz,
      peakMHz: firstMHz + r.peak * binMHz, level: r.level
    }));
}

export class SweepPipeline {
  constructor(plan, { clean = true, cleanup = false, threshold = plan.threshold, smoothingMs = 100 } = {}) {
    this.plan = plan;
    this.threshold = threshold;
    this.smoother = new SpectrumSmoother(smoothingMs);
    this.pipes = plan.centers.map(center => new Pipeline(validateConfig(center - plan.sliceHalfMHz, center + plan.sliceHalfMHz, plan.sampleCount, plan.sampleRate),
      { mode: "instant", window: "hann", spur: clean, cleanup, cleanupFrames: 16, diversity: false, raw: false }));
    this.size = Math.ceil(plan.spanMHz / plan.binMHz);
    this.sweepId = null;
    this.skippedSweeps = 0;
    this.calibrating = false;
    this.cleaning = cleanup;
    this.reset();
  }

  reset() {
    this.smoother.reset();
    this.entries = new Map();
    // Explicit resets discard the rest of the current sweep without counting it as an overload.
    this.finished = true;
    this.average = new Float64Array(this.size);
    this.peak = new Float64Array(this.size);
    this.visits = new Uint32Array(this.size);
    this.hits = new Uint32Array(this.size);
    this.sweeps = 0;
  }

  startCalibration() {
    for (const pipe of this.pipes) pipe.startCalibration(64);
    this.calibrating = true;
    this.reset();
  }

  setCleanup(enabled) {
    for (const pipe of this.pipes) pipe.setOptions({ cleanup: enabled });
    this.cleaning = enabled;
    this.reset(); // Do not retain unfiltered peaks/occupancy after a cleanup change.
  }

  cleanupStatus() {
    const statuses = this.pipes.map(pipe => pipe.cleanupStatus());
    return { on: statuses.every(s => s.on), state: statuses.some(s => s.state === "collecting") ? "collecting"
      : statuses.every(s => s.state === "ready") ? "ready" : "none",
      count: statuses.reduce((sum, s) => sum + (s.state === "ready" ? s.target : s.count), 0),
      target: statuses.reduce((sum, s) => sum + s.target, 0) };
  }

  clearCalibration() {
    for (const pipe of this.pipes) pipe.clearBaseline();
    this.calibrating = false;
    this.reset();
  }

  calibrationStatus() {
    return { state: this.calibrating ? "collecting" : this.pipes.every(pipe => pipe.baseline) ? "ready" : "none",
      count: this.pipes.reduce((sum, pipe) => sum + (pipe.baseline ? 64 : pipe.calibration?.entries.values().next().value.count ?? 0), 0),
      target: 64 * this.pipes.length };
  }

  process({ payload, centerMHz, sweepId, sliceIndex, captureIndex, retuned, bandwidth }) {
    const { plan } = this;
    if (retuned) return null;
    if (plan.centers[sliceIndex] !== centerMHz || !Number.isInteger(sweepId) || !Number.isInteger(captureIndex)
        || captureIndex < 0 || captureIndex >= plan.dwellFrames) throw new Error("Invalid sweep slice.");
    if (this.sweepId !== null && sweepId < this.sweepId) return null;
    if (sweepId !== this.sweepId) {
      if (this.sweepId !== null) this.skippedSweeps += Math.max(0, sweepId - this.sweepId - (this.finished ? 1 : 0));
      else this.skippedSweeps += sweepId;
      this.sweepId = sweepId;
      this.entries = new Map();
      this.finished = false;
    }
    const pipe = this.pipes[sliceIndex];
    if (pipe.updateBandwidth(bandwidth, centerMHz)) {
      // A filter change invalidates every stitched statistic and noise reference.
      this.clearCalibration();
      for (const item of this.pipes) if (item.options.cleanup) item.startCleanup();
      this.cleaning = this.pipes.some(item => item.options.cleanup);
      return { progress: 0, calibration: this.calibrationStatus(), cleanup: this.cleanupStatus(), bandwidthChanged: true };
    }
    const slice = pipe.process(payload, centerMHz, false, bandwidth);
    const half = Math.min(plan.sliceHalfMHz, bandwidth ? reliableHalfMHz(bandwidth.estimatedHz) : plan.sliceHalfMHz);
    for (let i = 0; i < slice.corrected.length; i++) {
      const offset = slice.firstMHz + i * slice.binMHz - centerMHz;
      if (Math.abs(offset) > half) slice.corrected[i] = NaN;
    }
    const progress = (sliceIndex * plan.dwellFrames + captureIndex + 1) / (plan.centers.length * plan.dwellFrames);
    if (this.calibrating) {
      if (this.pipes.every(pipe => pipe.baseline)) {
        this.calibrating = false;
        this.reset();
      }
      // Never publish rows containing both reference collection and corrected data.
      this.finished = true;
      return { progress, calibration: this.calibrationStatus(), cleanup: this.cleanupStatus() };
    }
    if (this.cleaning) {
      if (this.cleanupStatus().state === "ready") { this.cleaning = false; this.reset(); }
      // Reference captures never become mixed-unit rows, peaks or occupancy visits.
      this.finished = true;
      return { progress, calibration: this.calibrationStatus(), cleanup: this.cleanupStatus() };
    }
    if (this.finished) return null;
    let entry = this.entries.get(sliceIndex);
    if (!entry && captureIndex === 0) {
      entry = { count: 0, power: new Float64Array(slice.corrected.length), firstMHz: slice.firstMHz, binMHz: slice.binMHz };
      this.entries.set(sliceIndex, entry);
    }
    if (entry && entry.count === captureIndex) {
      for (let i = 0; i < entry.power.length; i++) entry.power[i] += 10 ** (slice.corrected[i] / 10);
      entry.count++;
    }
    const response = { progress, calibration: this.calibrationStatus(), cleanup: this.cleanupStatus(), skippedSweeps: this.skippedSweeps };
    if (sliceIndex !== plan.centers.length - 1 || captureIndex !== plan.dwellFrames - 1) return response;
    if (this.entries.size !== plan.centers.length || [...this.entries.values()].some(e => e.count !== plan.dwellFrames)) return response;
    this.finished = true;
    return { ...response, frame: this.finish() };
  }

  finish() {
    const { plan, size } = this, sums = new Float64Array(size), weights = new Float64Array(size);
    const meanSums = new Float64Array(size), medianSums = new Float64Array(size);
    for (const [sliceIndex, entry] of this.entries) {
      const center = plan.centers[sliceIndex];
      const first = Math.max(0, Math.floor((entry.firstMHz - plan.startMHz) / plan.binMHz));
      const last = Math.min(size, Math.ceil((center + plan.sliceHalfMHz - plan.startMHz) / plan.binMHz));
      for (let i = first; i < last; i++) {
        const fromMHz = plan.startMHz + i * plan.binMHz, frequency = fromMHz + plan.binMHz / 2;
        const from = Math.max(0, Math.ceil((fromMHz - entry.firstMHz) / entry.binMHz));
        const to = Math.min(entry.power.length, Math.ceil((fromMHz + plan.binMHz - entry.firstMHz) / entry.binMHz));
        const values = [];
        for (let bin = from; bin < to; bin++) if (Number.isFinite(entry.power[bin])) values.push(entry.power[bin] / entry.count);
        if (!values.length) continue;
        values.sort((a, b) => a - b);
        const weight = Math.min(1, Math.max(0.1, (plan.sliceHalfMHz - Math.abs(frequency - center)) / Math.max(1, plan.sliceHalfMHz * 0.15)));
        // Coarse overview cells retain their strongest FFT bin, so short narrow peaks stay visible.
        sums[i] += values.at(-1) * weight;
        meanSums[i] += values.reduce((sum, value) => sum + value, 0) / values.length * weight;
        medianSums[i] += values[values.length >> 1] * weight;
        weights[i] += weight;
      }
    }
    const current = Float32Array.from(sums, (sum, i) => weights[i] ? db(sum / weights[i]) : NaN);
    const smoothed = this.smoother.update(current);
    const noise = localNoise(current, plan.binMHz);
    for (let i = 0; i < size; i++) {
      if (!weights[i]) continue;
      const power = sums[i] / weights[i];
      this.visits[i]++;
      this.average[i] += (power - this.average[i]) / this.visits[i];
      this.peak[i] = Math.max(this.peak[i], power);
      if (current[i] >= noise[i] + this.threshold) this.hits[i]++;
    }
    this.sweeps++;
    const average = Float32Array.from(this.average, (p, i) => this.visits[i] ? db(p) : NaN);
    const peak = Float32Array.from(this.peak, (p, i) => this.visits[i] ? db(p) : NaN);
    const ranges = Object.fromEntries(Object.entries({ current, smoothed, average, peak }).map(([key, values]) => {
      const finite = Array.from(values).filter(Number.isFinite).sort((a, b) => a - b);
      const floor = (finite[finite.length >> 1] ?? -94) - 6;
      return [key, [floor, Math.max((finite.at(-1) ?? -23) + 3, floor + 26)]];
    }));
    return { current, smoothed, average, peak, ranges,
      waterfall: Float32Array.from(meanSums, (sum, i) => weights[i] ? db(sum / weights[i]) : NaN),
      waterfallMedian: Float32Array.from(medianSums, (sum, i) => weights[i] ? db(sum / weights[i]) : NaN),
      occupancy: Float32Array.from(this.hits, (n, i) => this.visits[i] ? n / this.visits[i] * 100 : NaN),
      regions: signalRegions(current, noise, plan.startMHz + plan.binMHz / 2, plan.binMHz, this.threshold),
      smoothedRegions: signalRegions(smoothed, localNoise(smoothed, plan.binMHz), plan.startMHz + plan.binMHz / 2, plan.binMHz, this.threshold),
      unit: this.cleanupStatus().state === "ready" || this.calibrationStatus().state === "ready" ? "dB above reference" : "dBFS",
      firstMHz: plan.startMHz + plan.binMHz / 2, binMHz: plan.binMHz, sweeps: this.sweeps, skippedSweeps: this.skippedSweeps };
  }
}
