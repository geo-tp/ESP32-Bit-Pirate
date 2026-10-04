// SPDX-License-Identifier: MIT
import { WATERFALL_ROWS, RECOMMENDED_BAND } from "./SdrProtocol.js";
import { aggregateBins } from "./Aggregation.js";

const themes = {
  dark: {
    stops: [[4, 12, 34], [10, 60, 150], [0, 180, 210], [250, 220, 45], [245, 70, 30]],
    bg: "#0a111b", plotBg: "#040c22", grid: "#96b6d21a", text: "#92a8bc", line: "#52d6c8", raw: "#6b7b88"
  },
  light: {
    stops: [[244, 248, 253], [150, 195, 235], [40, 160, 190], [240, 170, 20], [200, 30, 30]],
    bg: "#ffffff", plotBg: "#f4f8fd", grid: "#4a6a8a26", text: "#4d6275", line: "#0b7f93", raw: "#a3b1bd"
  }
};
for (const theme of Object.values(themes)) {
  theme.palette = Array.from({ length: 256 }, (_, value) => {
    const position = value / 255 * 4, a = Math.min(3, Math.floor(position)), t = position - a;
    return theme.stops[a].map((v, channel) => Math.round(v + (theme.stops[a + 1][channel] - v) * t));
  });
}
const currentTheme = () => themes[document.documentElement.dataset.theme === "light" ? "light" : "dark"];

// Tick spacing giving roughly 4-8 labels for the visible dB range.
function niceStep(range) {
  for (const step of [1, 2, 5, 10, 20, 25, 50]) if (range / step <= 8) return step;
  return 100;
}

export class SdrPlots {
  constructor(waterfall, spectrum, readout, waterfallReadout = null) {
    this.waterfall = waterfall;
    this.spectrum = spectrum;
    this.readout = readout;
    this.waterfallReadout = waterfallReadout;
    this.history = document.createElement("canvas");
    this.history.height = WATERFALL_ROWS;
    this.historyContext = this.history.getContext("2d");
    this.config = { startMHz: 2425, endMHz: 2449, spanMHz: 24 };
    this.latest = null;
    this.view = "corrected";
    this.scale = { floor: -100, ceiling: 0 };
    this.unit = "dBFS";
    this.showRaw = true;
    this.waterfallAggregation = "mean";
    this.scanView = "current";
    this.guideSets = { wifi: false, ble: false, zigbee: false };
    this.onInspect = null;
    this.rows = 0;
    this.head = 0;
    this.resizeObserver = new ResizeObserver(() => this.draw());
    this.resizeObserver.observe(waterfall);
    this.resizeObserver.observe(spectrum);
    this.levelRows = new Array(WATERFALL_ROWS);
    new MutationObserver(() => { this.repaintHistory(); this.draw(); })
      .observe(document.documentElement, { attributes: true, attributeFilter: ["data-theme"] });
    const frequencyAtPointer = (canvas, event) => {
      const rect = canvas.getBoundingClientRect(), { left, width, top, height } = this.geometry(canvas);
      const x = event.clientX - rect.left - left, y = event.clientY - rect.top;
      if (x < 0 || x > width || y < top || y > top + height) return null;
      return this.config.startMHz + Math.max(0, Math.min(1, x / width)) * this.config.spanMHz;
    };

    spectrum.addEventListener("pointermove", event => {
      this.inspecting = true;
      if (!this.latest) return;
      const frequency = frequencyAtPointer(spectrum, event);
      if (frequency === null) { this.updateReadout(); return; }
      const power = this.levels(this.latest);
      const index = Math.max(0, Math.min(power.length - 1,
        Math.round((frequency - this.latest.firstMHz) / this.latest.binMHz)));
      this.readout.textContent = `${(this.latest.firstMHz + index * this.latest.binMHz).toFixed(3)} MHz · ${Number.isFinite(power[index]) ? `${power[index].toFixed(1)} ${this.scanView === "occupancy" ? "% of visits" : this.unit}` : "outside valid coverage"}`;
    });
    spectrum.addEventListener("pointerleave", () => {
      this.inspecting = false;
      this.updateReadout();
    });

    if (this.waterfallReadout) {
      waterfall.addEventListener("pointermove", event => {
        const frequency = frequencyAtPointer(waterfall, event);
        this.waterfallReadout.textContent = frequency === null ? "" : `${frequency.toFixed(3)} MHz`;
      });
      waterfall.addEventListener("pointerleave", () => {
        this.waterfallReadout.textContent = "";
      });
    }
    for (const canvas of [waterfall, spectrum]) canvas.addEventListener("click", event => {
      if (!this.onInspect || !this.latest) return;
      const { left, width, top, height } = this.geometry(canvas), rect = canvas.getBoundingClientRect();
      const x = event.clientX - rect.left - left, y = event.clientY - rect.top;
      if (x < 0 || x > width || y < top || y > top + height) return;
      const frequency = this.config.startMHz + x / width * this.config.spanMHz;
      const levels = this.scanView === "current" ? this.levels(this.latest) : this.latest.current;
      let index = Math.round((frequency - this.latest.firstMHz) / this.latest.binMHz);
      if (!Number.isFinite(levels?.[index])) return;
      const radius = Math.max(1, Math.ceil(3 / width * this.config.spanMHz / this.latest.binMHz));
      const end = Math.min(levels.length - 1, index + radius);
      for (let i = Math.max(0, index - radius); i <= end; i++) {
        if (levels[i] > levels[index]) index = i;
      }
      this.onInspect(this.latest.firstMHz + index * this.latest.binMHz);
    });
  }

  reset(config) {
    this.config = config;
    this.rows = this.head = 0;
    this.latest = null;
    this.levelRows.fill(undefined);
    this.historyContext.clearRect(0, 0, this.history.width, WATERFALL_ROWS);
    this.readout.textContent = this.onInspect ? "" : `Hann window · ${(this.config?.sampleRate || 80000000) / 1e6} MS/s`;
    if (this.waterfallReadout) this.waterfallReadout.textContent = "";
    this.draw();
  }

  updateReadout() {
    if (!this.inspecting) this.readout.textContent = this.onInspect ? ""
      : `${this.latest?.status?.window === "blackman-harris" ? "Blackman-Harris" : "Hann"} · ${(this.config?.sampleRate || 80000000) / 1e6} MS/s`;
  }

  colorize(levels, rgbOut) {
    const { palette } = currentTheme(), { floor, ceiling } = this.scale, gain = 255 / (ceiling - floor);
    for (let x = 0; x < levels.length; x++) {
      if (!Number.isFinite(levels[x])) { rgbOut.set([0, 0, 0, 0], x * 4); continue; }
      const index = Math.max(0, Math.min(255, Math.floor((levels[x] - floor) * gain)));
      rgbOut.set([...palette[index], 255], x * 4);
    }
  }

  repaintHistory() {
    for (let row = 0; row < WATERFALL_ROWS; row++) {
      const levels = this.levelRows[row];
      if (!levels || levels.length !== this.history.width) continue;
      const line = this.historyContext.createImageData(levels.length, 1);
      this.colorize(levels, line.data);
      this.historyContext.putImageData(line, 0, row);
    }
  }

  levels(frame) {
    return frame.current ? (this.scanView === "current" ? frame.smoothed ?? frame.current : frame[this.scanView])
      : frame[this.view] ?? frame.corrected;
  }

  setScale(floor, ceiling, unit = this.unit) {
    this.scale = { floor, ceiling: Math.max(ceiling, floor + 1) };
    this.unit = unit;
    this.repaintHistory();
    this.draw();
  }

  setView(view) {
    this.view = view;
    this.rows = this.head = 0;
    this.levelRows.fill(undefined);
    this.historyContext.clearRect(0, 0, this.history.width, WATERFALL_ROWS);
    this.draw();
  }

  append(frame) {
    this.latest = frame;
    this.updateReadout();
    if (!frame.current && this.view === "corrected" && !frame.analysisFresh) return false;
    // A swept waterfall always shows current measurements, independent of peak/occupancy view.
    const power = frame.current ? (this.waterfallAggregation === "mean" ? frame.waterfall : this.waterfallAggregation === "median" ? frame.waterfallMedian : frame.current) ?? frame.current : this.levels(frame);
    const width = Math.min(2048, power.length);
    if (this.history.width !== width) this.history.width = width;
    const line = this.historyContext.createImageData(width, 1);
    const levels = aggregateBins(power, width, this.waterfallAggregation);
    this.colorize(levels, line.data);
    this.head = (this.head + WATERFALL_ROWS - 1) % WATERFALL_ROWS;
    this.levelRows[this.head] = levels;
    this.historyContext.putImageData(line, 0, this.head);
    this.rows = Math.min(WATERFALL_ROWS, this.rows + 1);
    return true;
  }

  geometry(canvas) {
    const w = canvas.clientWidth, h = canvas.clientHeight, left = w < 500 ? 48 : 58;
    return { w, h, left, top: 10, width: Math.max(1, w - left - 18), height: Math.max(1, h - 43) };
  }

  prepare(canvas) {
    const geometry = this.geometry(canvas), ratio = window.devicePixelRatio || 1;
    const width = Math.round(geometry.w * ratio), height = Math.round(geometry.h * ratio);
    if (canvas.width !== width || canvas.height !== height) { canvas.width = width; canvas.height = height; }
    const ctx = canvas.getContext("2d");
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    ctx.fillStyle = currentTheme().bg;
    ctx.fillRect(0, 0, geometry.w, geometry.h);
    ctx.font = "10px Menlo, monospace";
    return { ctx, ...geometry };
  }

  axes(plot, isWaterfall) {
    const { ctx, left, top, width, height, h } = plot;
    const ticks = width < 500 ? 4 : 8, { grid, text } = currentTheme();
    const { floor, ceiling } = !isWaterfall && this.scanView === "occupancy" ? { floor: 0, ceiling: 100 } : this.scale;
    ctx.lineWidth = 1;
    for (let i = 0; i <= ticks; i++) {
      const x = left + i * width / ticks;
      ctx.strokeStyle = grid;
      ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x, top + height); ctx.stroke();
      ctx.fillStyle = text; ctx.textAlign = i === 0 ? "left" : i === ticks ? "right" : "center";
      const value = this.config.startMHz + i * this.config.spanMHz / ticks;
      ctx.fillText(Number(value.toFixed(2)).toString(), x, h - 18);
    }
    ctx.textAlign = "right";
    if (isWaterfall) {
      for (let i = 0; i <= 4; i++) {
        const y = top + i * height / 4;
        ctx.fillStyle = text;
        ctx.fillText(String(i * 80), left - 9, Math.min(top + height, y + 4));
      }
    } else {
      const step = niceStep(ceiling - floor);
      for (let value = Math.floor(ceiling / step) * step; value >= floor; value -= step) {
        const y = top + (ceiling - value) / (ceiling - floor) * height;
        ctx.fillStyle = text;
        ctx.fillText(String(value), left - 9, Math.min(top + height, y + 4));
        ctx.strokeStyle = grid;
        ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(left + width, y); ctx.stroke();
      }
    }
    ctx.fillStyle = text; ctx.textAlign = "center";
    ctx.fillText("Frequency · MHz", left + width / 2, h - 3);
  }

  regions(plot) {
    const { ctx, left, top, width, height } = plot;
    const x = frequency => left + Math.max(0, Math.min(1, (frequency - this.config.startMHz) / this.config.spanMHz)) * width;
    const shade = (start, end, hatch) => {
      const a = x(start), b = x(end);
      if (b <= a) return;
      ctx.save(); ctx.beginPath(); ctx.rect(a, top, b - a, height); ctx.clip();
      ctx.fillStyle = hatch ? "#91a8bd12" : "#d39a3524";
      ctx.fillRect(a, top, b - a, height);
      if (hatch) {
        ctx.strokeStyle = "#91a8bd44"; ctx.lineWidth = 1;
        ctx.beginPath();
        for (let i = a - height; i < b; i += 10) { ctx.moveTo(i, top + height); ctx.lineTo(i + height, top); }
        ctx.stroke();
      }
      ctx.restore();
    };
    shade(this.config.startMHz, RECOMMENDED_BAND[0], false);
    shade(RECOMMENDED_BAND[1], this.config.endMHz, false);
    if (!this.onInspect) {
      const status = this.latest?.status;
      const center = this.config.centerMHz ?? (this.config.startMHz + this.config.endMHz) / 2;
      shade(this.config.startMHz, center + (status?.confidenceLower ?? -this.config.spanMHz * 0.4), true);
      shade(center + (status?.confidenceUpper ?? this.config.spanMHz * 0.4), this.config.endMHz, true);
    }
    ctx.save(); ctx.strokeStyle = "#c28d3e88"; ctx.setLineDash([4, 4]);
    for (const f of RECOMMENDED_BAND) if (f > this.config.startMHz && f < this.config.endMHz) {
      ctx.beginPath(); ctx.moveTo(x(f), top); ctx.lineTo(x(f), top + height); ctx.stroke();
    }
    ctx.restore();
  }

  draw() {
    const water = this.prepare(this.waterfall);
    const { ctx, left, top, width, height } = water;
    ctx.fillStyle = currentTheme().plotBg;
    ctx.fillRect(left, top, width, height);
    if (this.rows) {
      ctx.imageSmoothingEnabled = false;
      const first = Math.min(this.rows, WATERFALL_ROWS - this.head);
      ctx.drawImage(this.history, 0, this.head, this.history.width, first,
        left, top, width, first * height / WATERFALL_ROWS);
      if (this.rows > first) ctx.drawImage(this.history, 0, 0, this.history.width, this.rows - first,
        left, top + first * height / WATERFALL_ROWS, width, (this.rows - first) * height / WATERFALL_ROWS);
    }
    this.regions(water);
    this.axes(water, true);
    const plot = this.prepare(this.spectrum);
    this.regions(plot);
    this.axes(plot, false);
    if (!this.latest) return;
    const theme = currentTheme();
    const line = plot.ctx;
    line.save();
    line.beginPath(); line.rect(plot.left, plot.top, plot.width, plot.height); line.clip();
    if (this.latest.regions) {
      const regions = this.scanView === "current" ? this.latest.smoothedRegions ?? this.latest.regions : this.latest.regions;
      for (const region of regions) {
        const x = plot.left + (region.startMHz - this.config.startMHz) / this.config.spanMHz * plot.width;
        line.fillStyle = `${theme.line}18`;
        line.fillRect(x, plot.top, Math.max(3, (region.endMHz - region.startMHz) / this.config.spanMHz * plot.width), plot.height);
      }
      const guideSets = this.guideSets || {};
      const guideLines = [];
      if (guideSets.wifi) guideLines.push(
        [2412, "Wi-Fi 1"], [2437, "Wi-Fi 6"], [2462, "Wi-Fi 11"]
      );
      if (guideSets.ble) guideLines.push(
        [2402, "BLE 37"], [2426, "BLE 38"], [2480, "BLE 39"]
      );
      if (guideSets.zigbee) {
        for (let channel = 11; channel <= 26; channel++)
          guideLines.push([2405 + (channel - 11) * 5, `15.4 ${channel}`]);
      }
      for (const [frequency, label] of guideLines) {
        const x = plot.left + (frequency - this.config.startMHz) / this.config.spanMHz * plot.width;
        if (x < plot.left || x > plot.left + plot.width) continue;
        line.strokeStyle = theme.grid; line.setLineDash([3, 4]);
        line.beginPath(); line.moveTo(x, plot.top); line.lineTo(x, plot.top + plot.height); line.stroke(); line.setLineDash([]);
        line.fillStyle = theme.text; line.textAlign = "center"; line.fillText(label, x, plot.top + 10);
      }
    }
    if (this.showRaw && this.view !== "raw" && this.unit === "dBFS") this.trace(plot, this.latest.raw, theme.raw, 1);
    this.trace(plot, this.levels(this.latest), theme.line, 1.25);
    line.restore();
  }

  trace(plot, power, color, lineWidth) {
    const { firstMHz, binMHz } = this.latest;
    const { floor, ceiling } = this.scanView === "occupancy" ? { floor: 0, ceiling: 100 } : this.scale;
    const line = plot.ctx;
    line.strokeStyle = color; line.lineWidth = lineWidth;
    line.beginPath();
    const stride = Math.max(1, Math.floor(power.length / plot.width));
    let connected = false;
    for (let i = 0; i < power.length; i += stride) {
      let peak = -Infinity;
      for (let bin = i; bin < Math.min(power.length, i + stride); bin++) if (Number.isFinite(power[bin])) peak = Math.max(peak, power[bin]);
      if (!Number.isFinite(peak)) { connected = false; continue; }
      const x = plot.left + (firstMHz + i * binMHz - this.config.startMHz) / this.config.spanMHz * plot.width;
      const y = plot.top + (ceiling - Math.max(floor, Math.min(ceiling, peak))) / (ceiling - floor) * plot.height;
      if (connected) line.lineTo(x, y); else line.moveTo(x, y);
      connected = true;
    }
    line.stroke();
  }
}
