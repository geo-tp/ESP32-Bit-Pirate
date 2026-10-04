// SPDX-License-Identifier: MIT

// Average linear power over elapsed time, so faster sweeps do not speed up the trace.
export class SpectrumSmoother {
  constructor(responseMs = 100) {
    this.setResponse(responseMs);
  }

  setResponse(responseMs) {
    if (!Number.isFinite(responseMs) || responseMs < 0) throw new Error("Invalid spectrum smoothing time.");
    this.responseMs = responseMs;
    this.reset();
  }

  reset() {
    this.power = null;
    this.updatedAt = null;
  }

  update(levels, now = performance.now()) {
    if (this.power?.length !== levels.length) this.reset();
    const alpha = this.updatedAt === null || this.responseMs === 0 ? 1
      : -Math.expm1(-Math.max(0, now - this.updatedAt) / this.responseMs);
    this.updatedAt = now;
    this.power ??= new Float64Array(levels.length).fill(NaN);
    return Float32Array.from(levels, (level, i) => {
      // A missing observation must stay blank and must not carry an old signal forward.
      if (!Number.isFinite(level)) { this.power[i] = NaN; return NaN; }
      const power = 10 ** (level / 10);
      this.power[i] = Number.isFinite(this.power[i]) ? this.power[i] + alpha * (power - this.power[i]) : power;
      return 10 * Math.log10(Math.max(1e-20, this.power[i]));
    });
  }
}
