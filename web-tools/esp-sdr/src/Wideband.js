// SPDX-License-Identifier: MIT
import { planSweep } from "./Sweep.js";
import { bandwidthMHz, bandwidthSetting, experimentalRange, formatMHz, tuningLimits } from "./SdrProtocol.js";
import { SdrPlots } from "./SdrPlots.js";
import { setupAdvancedSettings } from "./AdvancedSettings.js";
import { updateGainControl } from "./GainControl.js";

const ids = ["scanRegionHint", "scanRxSummary", "scanBandwidth", "scanBandwidthHelp", "scanRangeField", "scanFrom", "scanTo", "scanFromValue", "scanToValue", "scanProfile", "scanRate", "scanGainMode", "scanGainIndex", "scanSummary", "scanHint", "scanError", "scanStart", "scanStop",
  "scanAdvanced", "scanClean", "scanAutoScale", "scanGuides", "scanBleGuides", "scanZigbeeGuides", "scanAggregation", "scanSmoothing", "scanReset", "scanClearCalibration",
  "scanCleanButton", "scanCalibrate", "scanCalibrationSummary", "scanCalibrationGuide", "scanCalibrationTitle", "scanCalibrationText",
  "scanCalibrationStart", "scanCalibrationDone", "scanCalibrationCancel", "scanCalibrationProgress", "scanTechnical",
  "scanUnit", "scanSpectrum", "scanWaterfall", "scanEmpty", "scanScaleMin", "scanScaleMax",
  "scanReadout", "scanWaterfallReadout", "scanSettingsStatus", "scanReceiverState"];
const setText = (element, value) => { if (element.textContent !== value) element.textContent = value; };

export class Wideband {
  constructor({ getClient, isReady, onStart, onEnd, onError, onInspect, onUiChange, getExtendedRange }) {
    Object.assign(this, { getClient, isReady, onStart, onEnd, onError, onInspect, onUiChange, getExtendedRange });
    this.ui = Object.fromEntries(ids.map(id => [id, document.getElementById(id)]));
    const u = this.ui;
    this.settings = setupAdvancedSettings(u.scanAdvanced);
    this.plots = new SdrPlots(u.scanWaterfall, u.scanSpectrum, u.scanReadout, u.scanWaterfallReadout);
    this.plots.showRaw = false;
    this.plots.onInspect = onInspect;
    this.cleanup = false;
    this.cleanupStatus = { state: "none" };
    this.active = false;
    this.generation = 0;
    this.phase = "idle";
    this.calibration = { state: "none" };
    this.queue = []; this.pendingFrames = [];
    this.busy = false;
    this.latest = null;
    this.autoScale = null;
    u.scanStart.addEventListener("click", () => this.start());
    u.scanStop.addEventListener("click", () => this.stop().catch(onError));
    const hintStopBeforeRangeChange = event => {
      if (!this.active) return;
      event.preventDefault();
      event.stopPropagation();
      u.scanStop.classList.remove("needs-stop-hint");
      void u.scanStop.offsetWidth;
      u.scanStop.classList.add("needs-stop-hint");
      clearTimeout(this.stopHintTimer);
      this.stopHintTimer = setTimeout(() => u.scanStop.classList.remove("needs-stop-hint"), 900);
    };
    u.scanRangeField.addEventListener("pointerdown", hintStopBeforeRangeChange, { capture: true });
    for (const input of [u.scanFrom, u.scanTo, u.scanProfile, u.scanBandwidth, u.scanRate]) input.addEventListener("input", () => {
      if (input === u.scanRate) this.rateEdited = true;
      if ([u.scanFrom, u.scanTo].includes(input)) { this.rangeEdited = true; this.syncRangeControl(input); this.selectRangeProfile(); }
      this.updatePlan(true);
    });
    const guideInputs = [u.scanGuides, u.scanBleGuides, u.scanZigbeeGuides];
    const syncGuides = changed => {
      if (changed?.checked) {
        for (const input of guideInputs) if (input !== changed) input.checked = false;
      }
      this.plots.guideSets = {
        wifi: u.scanGuides.checked,
        ble: u.scanBleGuides.checked,
        zigbee: u.scanZigbeeGuides.checked
      };
      this.plots.draw();
    };
    for (const input of guideInputs) input.addEventListener("change", () => syncGuides(input));
    syncGuides();
    u.scanAggregation.addEventListener("change", () => { this.plots.waterfallAggregation = u.scanAggregation.value; });
    u.scanSmoothing.addEventListener("change", () => {
      if (this.active && !this.preparing) this.worker?.postMessage({ smoothingMs: Number(u.scanSmoothing.value) });
    });
    u.scanGainMode.addEventListener("change", () => { u.scanGainIndex.disabled = u.scanGainMode.value !== "MANUAL"; this.updateControls(); });
    u.scanGainIndex.addEventListener("input", () => this.updateControls());
    u.scanAutoScale.addEventListener("change", () => this.draw());
    u.scanReset.addEventListener("click", () => this.send({ reset: true }));
    u.scanClearCalibration.addEventListener("click", () => { this.calibration = { state: "none" }; this.send({ clearCalibration: true }); this.updateCalibration(); });
    for (const button of document.querySelectorAll("[data-scan-view]")) button.addEventListener("click", () => {
      this.plots.scanView = button.dataset.scanView; this.autoScale = null;
      for (const item of document.querySelectorAll("[data-scan-view]")) item.setAttribute("aria-pressed", String(item === button));
      this.draw();
    });
    u.scanCleanButton.addEventListener("click", () => {
      if (!this.active || this.preparing || this.phase !== "idle") return;
      this.cleanup = !this.cleanup;
      this.cleanupStatus = { state: this.cleanup ? "collecting" : "none", count: 0, target: this.plan.centers.length * 16 };
      this.send({ cleanup: this.cleanup }); this.updateControls();
    });
    u.scanCalibrate.addEventListener("click", () => {
      if (this.preparing || this.phase !== "idle" || (!this.active && !this.isReady())) return;
      u.scanAdvanced.open = true;
      this.settings.select("calibration");
      if (!this.active) this.start();
      this.phase = "prepare";
      this.updateCalibration();
      this.onUiChange();
      if (!u.scanCalibrationStart.disabled) u.scanCalibrationStart.focus();
    });
    u.scanCalibrationStart.addEventListener("click", () => {
      if (!this.active || this.phase !== "prepare") return;
      this.phase = "collecting"; this.calibration = { state: "collecting", count: 0, target: this.plan.centers.length * 64 };
      this.cleanup = false; this.cleanupStatus = { state: "none" }; this.send({ cleanup: false });
      u.scanClean.checked = true;
      this.send({ calibrate: true, clean: true }); this.updateCalibration(); this.onUiChange(); u.scanCalibrationCancel.focus();
    });
    u.scanCalibrationCancel.addEventListener("click", () => {
      if (this.phase === "collecting") {
        this.phase = "interrupted"; this.calibration = { state: "none" }; this.send({ clearCalibration: true });
      } else this.phase = "idle";
      this.updateCalibration(); this.onUiChange();
      (this.phase === "idle" ? u.scanCalibrate : u.scanCalibrationDone).focus();
    });
    u.scanCalibrationDone.addEventListener("click", () => {
      this.phase = "idle"; this.updateControls(); this.onUiChange();
      if (this.active) u.scanCalibrate.focus();
    });
    this.syncRangeControl();
    this.updatePlan();
  }

  rangeLimits() {
    return tuningLimits(this.getClient()?.info, Boolean(this.getExtendedRange?.()));
  }

  rangeModeChanged() {
    if (this.active) {
      this.pendingRangeModeChange = true;
      return;
    }
    this.pendingRangeModeChange = false;
    const u = this.ui, [min, max] = this.rangeLimits();
    const outside = u.scanFrom.valueAsNumber < min || u.scanTo.valueAsNumber > max;
    for (const input of [u.scanFrom, u.scanTo]) {
      input.min = String(min); input.max = String(max);
    }
    if (outside) {
      u.scanFrom.value = String(Math.max(min, Math.min(max - 2, 2330)));
      u.scanTo.value = String(Math.min(max, Math.max(min + 2, 2550)));
      this.rangeEdited = false;
    }
    this.syncRangeControl();
    this.selectRangeProfile();
    this.updatePlan(true);
  }

  syncRangeControl(changed = null) {
    const u = this.ui, min = Number(u.scanFrom.min || 2200), max = Number(u.scanFrom.max || 2800), gap = 2;
    let from = u.scanFrom.valueAsNumber, to = u.scanTo.valueAsNumber;
    if (changed === u.scanFrom && from > to - gap) from = to - gap;
    if (changed === u.scanTo && to < from + gap) to = from + gap;
    from = Math.max(min, Math.min(max - gap, from)); to = Math.min(max, Math.max(min + gap, to));
    u.scanFrom.value = from; u.scanTo.value = to;
    u.scanFromValue.value = Number(from.toFixed(1)); u.scanToValue.value = Number(to.toFixed(1));
    const left = (from - min) / (max - min) * 100, right = (to - min) / (max - min) * 100;
    u.scanFrom.parentElement.style.setProperty("--range-left", `${left}%`);
    u.scanFrom.parentElement.style.setProperty("--range-right", `${right}%`);
  }

  selectRangeProfile() {
    const u = this.ui;
    const from = u.scanFrom.valueAsNumber, to = u.scanTo.valueAsNumber;
    const [min, max] = this.rangeLimits();
    if (!Number.isFinite(from) || !Number.isFinite(to) || to - from < 2
      || from < min || to > max) return;
    const span = to - from;
    u.scanProfile.value = span < 100 ? "sensitive" : span < 300 ? "balanced" : "fast";
  }

  updatePlan(clear = false) {
    const u = this.ui, info = this.getClient()?.info;
    if (info && !this.active) {
      if (!this.rateEdited) u.scanRate.value = String(Number(info.fs_hz) / 1e6);
      u.scanGainIndex.max = String(Number(info.gain_max ?? 82));
    }
    const [min, max] = this.rangeLimits();
    for (const input of [u.scanFrom, u.scanTo]) {
      input.min = String(min); input.max = String(max);
    }
    if (!this.rangeEdited && !this.active) {
      const from = Math.max(min, Math.min(max - 2, 2330));
      const to = Math.min(max, Math.max(min + 2, 2550));
      const changed = Number(u.scanFrom.value) !== from || Number(u.scanTo.value) !== to;
      u.scanFrom.value = from; u.scanTo.value = to;
      if (changed) this.selectRangeProfile();
    }
    this.syncRangeControl();
    try {
      if (this.getClient()?.supportsBandwidth) bandwidthSetting(u.scanBandwidth.value, info);
      const planInfo = { ...(info ?? {}), center_min_mhz: String(min), center_max_mhz: String(max), fs_hz: String(Number(u.scanRate.value) * 1_000_000) };
      const plan = planSweep(Number(u.scanFrom.value), Number(u.scanTo.value), u.scanProfile.value, planInfo, this.getClient()?.bandwidth);
      if (!this.active) this.plan = plan;
      u.scanError.hidden = true;
      u.scanFrom.removeAttribute("aria-invalid"); u.scanTo.removeAttribute("aria-invalid");
      u.scanSummary.textContent = `${plan.spanMHz} MHz · ${plan.binMHz} MHz detail`;
      u.scanHint.textContent = { fast: "Fast", balanced: "Fine", sensitive: "Averaged" }[plan.profile]
        + (plan.spanMHz > 120 ? " · slower refresh" : "");
      u.scanHint.title = "Wider scan ranges take longer to refresh. Sensitive detail averages more captures per frequency.";
      u.scanTechnical.textContent = `${plan.centers.length} centers · up to ${formatMHz(plan.stepMHz)} MHz steps · ${formatMHz(plan.sliceHalfMHz * 2)} MHz central slices · ${plan.sampleCount.toLocaleString()} samples · ${plan.dwellFrames} captures per center · ${(plan.fftBinMHz * 1000).toFixed(1)} kHz FFT bins grouped into ${plan.binMHz} MHz activity cells · activity threshold: ${plan.threshold} dB above local noise`;
      const experimental = experimentalRange(plan.startMHz, plan.endMHz);
      u.scanRegionHint.textContent = experimental ? "Experimental regions" : "2.4 GHz ISM";
      u.scanRegionHint.title = experimental ? "Amber areas outside 2400–2483.5 MHz are experimental / uncharacterized. Reception may be unreliable."
        : "Recommended / characterized RF band: 2400–2483.5 MHz.";
      u.scanRegionHint.setAttribute("aria-label", u.scanRegionHint.title);
      u.scanRegionHint.classList.toggle("experimental", experimental);
      u.scanRxSummary.textContent = `RX bandwidth ${this.getClient()?.bandwidth?.estimatedHz ? `≈${formatMHz(bandwidthMHz(this.getClient().bandwidth))} MHz` : "checked on Start"} · sample rate ${u.scanRate.value} MS/s`;
      if (clear || !this.latest && !this.active) {
        this.latest = null; this.autoScale = null; this.calibration = { state: "none" };
            this.plots.reset(plan);
        u.scanEmpty.hidden = false;
      }
    } catch (error) {
      u.scanError.textContent = error.message; u.scanError.hidden = false;
      u.scanFrom.setAttribute("aria-invalid", "true"); u.scanTo.setAttribute("aria-invalid", "true");
    }
    this.updateControls();
  }

  updateControls() {
    const u = this.ui;
    u.scanStart.disabled = this.active || !this.isReady() || !u.scanError.hidden || this.phase !== "idle";
    u.scanStop.disabled = !this.active;
    for (const input of [u.scanFrom, u.scanTo, u.scanProfile, u.scanClean, u.scanRate, u.scanGainMode, u.scanGainIndex]) input.disabled = this.active || (input === u.scanRate && !this.getClient()?.supportsRate) || (input === u.scanGainMode && !this.getClient()?.supportsGain) || (input === u.scanGainIndex && (!this.getClient()?.supportsGain || u.scanGainMode.value !== "MANUAL"));
    u.scanRangeField.classList.toggle("is-scan-locked", this.active);
    u.scanBandwidth.disabled = this.active || !this.getClient()?.supportsBandwidth;
    updateGainControl(u.scanGainMode, u.scanGainIndex);
    setText(u.scanReceiverState, this.active ? "Stop the scan to change receiver settings."
      : !this.getClient()?.info ? "Connect your adapter to check available receiver controls."
      : "Receiver settings are applied when you start the scan.");
    u.scanBandwidthHelp.textContent = !this.getClient()?.info
      ? "Wide requests the broadest available receive filter. Auto lets the firmware choose it after connecting."
      : this.getClient().supportsBandwidth
      ? "Wide requests the broadest available receive filter. Auto lets the firmware choose it. The scan checks the actual bandwidth on Start and combines overlapping central slices."
      : "RX bandwidth is unknown on this firmware. Scanning uses conservative overlapping slices until updated firmware reports the filter width.";
    u.scanReset.disabled = !this.active || this.preparing || this.phase !== "idle";
    this.updateCalibration();
  }

  updateCalibration() {
    const u = this.ui, c = this.calibration;
    this.settings.setCalibrating(this.phase !== "idle");
    u.scanCalibrate.disabled = this.preparing || this.phase !== "idle" || (!this.active && !this.isReady());
    u.scanClearCalibration.disabled = !this.active || c.state !== "ready" || this.phase !== "idle";
    u.scanCalibrationGuide.hidden = this.phase === "idle";
    u.scanCalibrate.setAttribute("aria-expanded", String(this.phase !== "idle"));
    setText(u.scanCalibrate, c.state === "ready" ? "Recalibrate…" : "Calibrate…");
    u.scanCleanButton.disabled = !this.active || this.preparing || this.phase !== "idle";
    u.scanCleanButton.setAttribute("aria-pressed", String(this.cleanup));
    const clean = this.cleanupStatus;
    u.scanCleanButton.setAttribute("aria-busy", String(this.cleanup && this.active && clean.state !== "ready"));
    setText(u.scanCleanButton.querySelector(".action-label"), this.cleanup && this.active && clean.state !== "ready"
      ? `Reducing… ${Math.round((clean.count || 0) / (clean.target || 1) * 100)}%` : "Reduce noise");
    setText(u.scanCleanButton.querySelector(".action-state"), !this.cleanup ? "OFF" : clean.state === "ready" ? "ON" : "…");
    u.scanCleanButton.title = this.cleanup ? "Live background reference: signals present during measurement may also be reduced. Click to cancel or remove it and compare the original signal."
      : "Measure the background at every tuning center with the antenna connected. Signals present during measurement may also be reduced.";
    u.scanCalibrate.title = c.state === "ready" ? "Receiver calibration applied. Click to repeat." : "Optional noise-reference calibration with the antenna disconnected.";
    setText(u.scanCalibrationSummary, c.state === "ready" ? "Reference applied to every tuning center."
      : c.state === "collecting" ? `Measuring receiver noise · ${Math.round(c.count / c.target * 100)}% · keep the antenna disconnected.`
      : "Uses a noise-only reference at every tuning center. Calibrate starts the scan automatically.");
    const instructions = {
      prepare: ["Disconnect the antenna", "We’ll measure the receiver’s own noise to reduce artifacts. Disconnect the antenna or terminate the input first so real signals are not removed."],
      collecting: ["Measuring receiver noise…", "Keep the antenna disconnected. We’ll apply the correction automatically; wider ranges take longer."],
      reconnect: ["Reconnect your antenna", "Calibration is applied. Reconnect the antenna to view signals."],
      interrupted: ["Calibration interrupted", "The measurement was not completed. Reconnect your antenna before listening or scanning again."]
    }[this.phase];
    if (instructions) { setText(u.scanCalibrationTitle, instructions[0]); setText(u.scanCalibrationText, instructions[1]); }
    const bandwidthSummary = u.scanBandwidth.value === "WIDE" ? "Wide RX" : "Auto RX";
    const receiverSummary = `${u.scanRate.value} MS/s · ${bandwidthSummary}`;
    setText(u.scanSettingsStatus, instructions ? instructions[0] : receiverSummary + (c.state === "ready" ? " · Calibrated" : ""));
    u.scanCalibrationStart.hidden = this.phase !== "prepare";
    u.scanCalibrationStart.disabled = !this.active;
    u.scanCalibrationDone.hidden = !["reconnect", "interrupted"].includes(this.phase);
    u.scanCalibrationCancel.hidden = !["prepare", "collecting"].includes(this.phase);
    u.scanCalibrationProgress.hidden = this.phase !== "collecting";
    u.scanCalibrationProgress.value = c.count / c.target * 100 || 0;
  }

  start() {
    if (!this.isReady() || this.active || this.phase !== "idle") return;
    this.updatePlan();
    if (!this.ui.scanError.hidden) return;
    this.preparing = true; this.stopping = false;
    this.active = true; this.busy = false; this.queue = []; this.pendingFrames = []; this.latest = null; this.autoScale = null;
    this.calibration = { state: "none" };
    this.cleanupStatus = { state: this.cleanup ? "collecting" : "none", count: 0, target: 0 };
    this.generation++;
    this.plots.reset(this.plan);
    this.ui.scanWaterfall.dataset.sweeps = "0";
    this.ui.scanEmpty.hidden = false;
    this.worker = new Worker(new URL("../workers/sweep-worker.js", import.meta.url), { type: "module" });

    this.worker.onmessage = ({ data }) => {
      this.busy = false;
      if (data.error) { this.fail(data.error); return; }
      if (data.generation === this.generation && this.active) this.accept(data);
      this.pump();
    };
    this.worker.onerror = event => { event.preventDefault(); this.fail(event.message || "Scan processing failed."); };
    this.onStart(); this.updateControls();
    this.task = this.run(this.getClient());
  }

  async run(client) {
    try {
      await client.configureRate(Number(this.ui.scanRate.value));
      await client.configureGain(this.ui.scanGainMode.value, Number(this.ui.scanGainIndex.value));
      await client.configureBandwidth(this.ui.scanBandwidth.value);
      if (this.stopping || client.disconnecting || !client.info) return;
      await client.measureBandwidth(this.plan.centers[0]);
      if (this.stopping || client.disconnecting || !client.info) return;
      const [min, max] = this.rangeLimits();
      const planInfo = { ...client.info, center_min_mhz: String(min), center_max_mhz: String(max) };
      this.plan = planSweep(Number(this.ui.scanFrom.value), Number(this.ui.scanTo.value), this.ui.scanProfile.value, planInfo, client.bandwidth);
      this.preparing = false;
      this.updatePlan(); this.plots.reset(this.plan);
      this.worker.postMessage({ plan: this.plan, options: { clean: this.ui.scanClean.checked, cleanup: this.cleanup, smoothingMs: Number(this.ui.scanSmoothing.value) } });
      await client.sweep(this.plan, frame => {
        if (!frame.sweepValid || !this.active) return;
        if (this.queue.length >= 32) this.queue.shift();
        this.queue.push(frame); this.pump();
      });
    } catch (error) { this.onError(error); }
    finally {
      this.active = false; this.generation++;
      this.worker?.terminate(); this.worker = null; this.queue = []; this.pendingFrames = []; this.busy = false;
      if (this.cleanupStatus.state !== "ready") { this.cleanup = false; this.cleanupStatus = { state: "none" }; }
      if (["prepare", "collecting"].includes(this.phase)) { this.phase = "interrupted"; this.calibration = { state: "none" }; }
      this.onEnd();
      if (this.pendingRangeModeChange) this.rangeModeChanged();
      else this.updateControls();
    }
  }

  async stop() {
    if (!this.active) return;
    this.stopping = true;
    await this.getClient()?.stop();
    await this.task;
  }

  fail(message) {
    this.onError(new Error(message));
    this.worker?.terminate(); this.worker = null;
    this.queue = [];
    void this.getClient()?.disconnect().catch(this.onError);
  }

  pump() {
    if (this.busy || !this.worker || !this.queue.length) return;
    const frame = this.queue.shift(); this.busy = true;
    this.worker.postMessage({ payload: frame.payload, centerMHz: frame.centerHz / 1e6, sweepId: frame.sweepId,
      bandwidth: frame.bandwidth, sliceIndex: frame.sliceIndex, captureIndex: frame.captureIndex, generation: this.generation }, [frame.payload.buffer]);
  }

  send(message) {
    this.generation++; this.queue = []; this.pendingFrames = []; this.latest = null; this.autoScale = null;
    this.worker?.postMessage(message);
    this.plots.reset(this.plan);
    this.ui.scanWaterfall.dataset.sweeps = "0";
  }

  accept(data) {
    const u = this.ui;
    if (data.cleanup) this.cleanupStatus = data.cleanup;
    if (data.bandwidthChanged) {
      if (this.phase === "collecting") this.phase = "interrupted";
      this.latest = null; this.autoScale = null; this.pendingFrames = []; this.plots.reset(this.plan);
    }
    if (data.calibration) {
      this.calibration = data.calibration;
      if (this.phase === "collecting" && data.calibration.state === "ready") {
        const focused = document.activeElement === u.scanCalibrationCancel;
        this.phase = "reconnect"; this.updateCalibration(); this.onUiChange();
        if (focused) u.scanCalibrationDone.focus();
      } else this.updateCalibration();
    }
    if (!data.frame) return;
    if (this.pendingFrames.length >= 12) this.pendingFrames.shift();
    this.pendingFrames.push(data.frame);
    if (!this.renderQueued) {
      this.renderQueued = true;
      requestAnimationFrame(() => { this.renderQueued = false; this.render(); });
    }
  }

  render() {
    if (!this.active || !this.pendingFrames.length) return;
    const u = this.ui, frames = this.pendingFrames.splice(0);
    this.latest = frames.at(-1);
    for (const frame of frames) this.plots.append(frame);
    this.draw(); u.scanEmpty.hidden = true;
    u.scanWaterfall.dataset.sweeps = String(this.latest.sweeps);
  }

  draw() {
    if (!this.latest) { this.plots.draw(); return; }
    const u = this.ui, unit = this.latest.unit;
    const key = this.plots.scanView === "current" ? "smoothed" : this.plots.scanView === "occupancy" ? "current" : this.plots.scanView;
    if (u.scanAutoScale.checked || this.plots.unit !== unit) {
      const [rawFloor, rawCeiling] = this.latest.ranges[key];
      const snap = value => Math.round(value / 5) * 5;
      const target = { floor: snap(rawFloor), ceiling: snap(rawCeiling) };
      if (!this.autoScale || this.autoScale.unit !== unit || this.plots.scanView === "occupancy") this.autoScale = { ...target, unit };
      else {
        if (Math.abs(target.floor - this.autoScale.floor) >= 5) this.autoScale.floor += 0.04 * (target.floor - this.autoScale.floor);
        if (Math.abs(target.ceiling - this.autoScale.ceiling) >= 5) this.autoScale.ceiling += 0.04 * (target.ceiling - this.autoScale.ceiling);
      }
      const floor = snap(this.autoScale.floor), ceiling = snap(this.autoScale.ceiling);
      if (this.plots.unit !== unit || Math.abs(this.plots.scale.floor - floor) >= 5 || Math.abs(this.plots.scale.ceiling - ceiling) >= 5) this.plots.setScale(floor, ceiling, unit);
    }
    u.scanScaleMin.textContent = Math.round(this.plots.scale.floor);
    u.scanScaleMax.textContent = `${Math.round(this.plots.scale.ceiling)} ${unit === "dB above reference" ? "dB" : unit}`;
    u.scanUnit.textContent = this.plots.scanView === "occupancy" ? "Activity · % of visits" : unit === "dBFS" ? "Relative level · dBFS" : "Level above reference · dB";
    this.plots.draw();
  }
}
