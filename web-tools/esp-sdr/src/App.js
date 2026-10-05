// SPDX-License-Identifier: MIT
import { SdrClient } from "./SdrClient.js";
import { SdrPlots } from "./SdrPlots.js";
import { Wideband } from "./Wideband.js";
import { setupAdvancedSettings } from "./AdvancedSettings.js";
import { updateGainControl } from "./GainControl.js";
import { DIVERSITY_OFFSETS_MHZ } from "./Pipeline.js";
import { DEFAULT_SAMPLE_RATE, LEGACY_SPAN_MHZ, bandwidthMHz, bandwidthSetting, experimentalRange, formatMHz, tuningLimits, validateConfig } from "./SdrProtocol.js";

const ids = ["rxPreset", "rxExact", "rxSummary", "rxHelp", "liveRegionHint", "serialUnsupported", "connectButton", "disconnectButton", "antennaHint", "startButton", "stopButton",
  "liveTab", "wideTab", "livePanel", "widePanel", "advancedSettings", "liveSettingsStatus", "scanExtendedRange", "liveReceiverState", "liveAggregation",
  "connectionStatus", "centerFreq", "frequencyPresetToggle", "frequencyPresetMenu", "wifiFrequencyPresets", "sampleCount", "liveRate", "liveGainMode", "liveGainIndex", "rangeSummary", "binSummary", "displayHint",
  "configError", "waterfallCanvas", "spectrumCanvas", "cursorReadout", "emptyState",
  "viewSelect", "modeSelect", "alphaInput", "windowSelect", "spurToggle", "diversityToggle", "rawOverlay",
  "cleanButton", "calibrateButton", "clearBaselineButton", "resetButton", "peakResetButton", "autoScale", "scaleFloor", "scaleCeiling",
  "waterfallReadout",
  "calibrationLabel", "calibrationSummary", "calibrationGuide", "calibrationTitle", "calibrationText",
  "calibrationStartButton", "calibrationDoneButton", "calibrationCancelButton", "calibrationProgressWrap",
  "calibrationProgress", "calibrationProgressText",
  "processingState", "analysisText",
  "scaleMin", "scaleMax", "spectrumUnit", "streamStats", "statusMessage", "technicalLog"];
const ui = Object.fromEntries(ids.map(id => [id, document.getElementById(id)]));
const liveSettings = setupAdvancedSettings(ui.advancedSettings);
const supported = "serial" in navigator && window.isSecureContext;
const plots = new SdrPlots(ui.waterfallCanvas, ui.spectrumCanvas, ui.cursorReadout, ui.waterfallReadout);
const QUEUE_LIMIT = 12, BASELINE_FRAMES = 64;
let displayWidth = LEGACY_SPAN_MHZ;
let client = null, state = "disconnected", worker = null, workerBusy = false;
let preferredLiveGainMode = ui.liveGainMode.value;
let config = null, rawQueue = [], displayQueue = [], renderPending = false;
let received = 0, displayed = 0, dropped = 0, settling = 0, startedAt = 0, endedAt = 0;
let latest = null, workerFailure = null, bandwidthUiKey = null;
let liveBandwidths = new Map();
let generation = 0, revision = null;
let calibrationStep = "idle", calibrationStatus = null;
let workspaceMode = "wide", switching = false, preparingLive = false, cleanupEnabled = false, wideExtendedRangeEnabled = false;
const wideband = new Wideband({
  getClient: () => client,
  isReady: () => state === "connected" && workspaceMode === "wide" && !switching,
  onStart: () => { state = "scanning"; status(""); updateControls(); },
  onEnd: () => {
    if (state !== "disconnecting") {
      state = client?.info ? "connected" : "disconnected";
      if (state === "disconnected") client = null;
    }
    if (!ui.statusMessage.classList.contains("error")) status("");
    updateControls();
  },
  onError: error => status(error.message, true),
  onInspect: frequency => void switchWorkspace("live", frequency),
  onUiChange: () => updateControls(),
  getExtendedRange: () => wideExtendedRangeEnabled
});

function log(message) {
  const lines = ui.technicalLog.textContent.split("\n").filter(Boolean);
  lines.push(`${new Date().toLocaleTimeString()}  ${message}`);
  ui.technicalLog.textContent = lines.slice(-100).join("\n");
  ui.technicalLog.scrollTop = ui.technicalLog.scrollHeight;
}

function status(message, error = false) {
  ui.statusMessage.textContent = message;
  ui.statusMessage.classList.toggle("error", error);
  ui.statusMessage.hidden = !error || !message;
  if (error) log(`ERROR: ${message}`);
}

function readConfig() {
  const center = Number(ui.centerFreq.value);
  const [min, max] = liveLimits();
  if (!Number.isInteger(center) || center < min || center > max) {
    throw new Error(`Choose a center frequency between ${min} and ${max} MHz, in whole MHz (±${displayWidth / 2} MHz is displayed).`);
  }
  if (ui.diversityToggle.checked && (center - 2 < min || center + 2 > max)) {
    throw new Error(`Tune diversity needs 2 MHz of tuning margin; choose ${min + 2}–${max - 2} MHz or disable Tune diversity.`);
  }
  return validateConfig(center - displayWidth / 2, center + displayWidth / 2, Number(ui.sampleCount.value), selectedSampleRate());
}

function liveLimits() {
  // Live SDR is independent from Wideband's Extended range option.
  return tuningLimits(client?.info, true);
}

function extendedRangeAvailable() {
  const normal = tuningLimits(client?.info, false);
  const extended = tuningLimits(client?.info, true);
  return extended[0] < normal[0] || extended[1] > normal[1];
}

function selectedSampleRate() { return Number(ui.liveRate?.value || 80) * 1_000_000; }
function effectiveDisplayWidth() { return Math.min(bandwidthMHz(client?.bandwidth), selectedSampleRate() / 1e6); }

function adapterConnectionLabel(info) {
  const rawVersion = String(info?.adapter_version || info?.firmware_version || info?.fw_version || "").trim();
  const version = rawVersion ? (/^v/i.test(rawVersion) ? rawVersion : `v${rawVersion}`) : "BPRF1";
  return `ESP SDR Adapter · ${version}`;
}

function updateControls() {
  const locked = ["connecting", "streaming", "scanning", "stopping", "disconnecting"].includes(state);
  ui.connectButton.hidden = ["connected", "streaming", "scanning", "stopping", "disconnecting"].includes(state);
  if (client?.info) {
    ui.antennaHint.textContent = adapterConnectionLabel(client.info);
    ui.antennaHint.dataset.state = "connected";
    ui.antennaHint.title = "ESP SDR adapter connected";
  } else {
    ui.antennaHint.textContent = "Antenna quality affects SDR performance";
    ui.antennaHint.dataset.state = "hint";
    ui.antennaHint.title = "";
  }
  ui.antennaHint.hidden = false;
  ui.connectButton.disabled = !supported || locked;
  ui.disconnectButton.hidden = !ui.connectButton.hidden;
  ui.disconnectButton.disabled = state === "disconnecting";
  ui.startButton.disabled = state !== "connected" || switching || workspaceMode !== "live";
  ui.stopButton.disabled = state !== "streaming";
  ui.clearBaselineButton.disabled = state !== "streaming" || calibrationStatus?.baseline !== "ready" || calibrationStep !== "idle";
  ui.resetButton.disabled = ui.peakResetButton.disabled = state !== "streaming";
  for (const input of [ui.centerFreq, ui.sampleCount, ui.diversityToggle, ui.liveRate, ui.liveGainMode, ui.liveGainIndex]) {
    input.disabled = locked
      || (input === ui.liveRate && client && !client.supportsRate)
      || (input === ui.liveGainMode && client && !client.supportsGain)
      || (input === ui.liveGainIndex && (ui.liveGainMode.value !== "MANUAL" || client && !client.supportsGain));
  }
  ui.frequencyPresetToggle.disabled = ui.centerFreq.disabled;
  if (ui.frequencyPresetToggle.disabled) closeFrequencyPresets();
  updateGainControl(ui.liveGainMode, ui.liveGainIndex);
  setText(ui.liveReceiverState, locked ? "Stop the stream to change receiver settings."
    : !client?.info ? "Connect your adapter to check available receiver controls."
    : "Receiver settings are applied when you start the stream.");
  [ui.centerFreq.min, ui.centerFreq.max] = liveLimits();
  const canExtend = extendedRangeAvailable();
  ui.scanExtendedRange.checked = wideExtendedRangeEnabled;
  ui.scanExtendedRange.disabled = !canExtend;
  const calibrating = workspaceMode === "live" ? calibrationStep !== "idle" : wideband.phase !== "idle";
  ui.liveTab.disabled = ui.wideTab.disabled = switching || ["connecting", "stopping", "disconnecting"].includes(state) || calibrating;
  ui.windowSelect.disabled = calibrationStep === "collecting";
  ui.connectionStatus.textContent = { disconnected: "Disconnected", connecting: "Connecting…",
    connected: "Connected", streaming: "Live", scanning: "Scanning", stopping: "Stopping…", disconnecting: "Disconnecting…" }[state];
  ui.connectionStatus.dataset.state = ["streaming", "scanning"].includes(state) ? "live" : state;
  try {
    const next = readConfig();
    if (client?.supportsBandwidth) bandwidthSetting(ui.rxExact.value, client.info);
    ui.rangeSummary.textContent = `${formatMHz(next.startMHz)}–${formatMHz(next.endMHz)} MHz · live`;
    ui.binSummary.textContent = `${(next.sampleRate / next.sampleCount / 1000).toFixed(2)} kHz / bin · ${next.sampleRate / 1e6} MS/s acquisition`;
    ui.configError.hidden = true;
    ui.centerFreq.removeAttribute("aria-invalid");
    if (!plots.latest && !locked) plots.reset(next);
  } catch (error) {
    ui.rangeSummary.textContent = "Choose a valid center frequency";
    ui.binSummary.textContent = `±${displayWidth / 2} MHz displayed`;
    ui.configError.textContent = error.message;
    ui.configError.hidden = false;
    ui.centerFreq.setAttribute("aria-invalid", "true");
    ui.startButton.disabled = true;
  }
  const reported = client?.bandwidth?.estimatedHz, target = client?.bandwidth?.targetHz;
  ui.rxSummary.textContent = `Sample rate · ${selectedSampleRate() / 1e6} MS/s · RX bandwidth ${reported ? `≈${formatMHz(reported / 1e6)} MHz` : target ? `target ≈${formatMHz(target / 1e6)} MHz · awaiting capture` : "unknown · conservative 24 MHz view"}`;
  ui.rxPreset.disabled = ui.rxExact.disabled = locked || !client?.supportsBandwidth;
  ui.rxHelp.textContent = !client?.info ? "Choose an exact bandwidth in MHz, WIDE or AUTO after connecting. This sets the analog receive bandwidth, separately from the sample rate."
    : client.supportsBandwidth ? `Choose ${client.info.bw_min_mhz}–${client.info.bw_max_mhz} MHz, WIDE or AUTO. Applied on Start. Wider edges can be less reliable; this is an approximate analog bandwidth, not the sample rate.`
    : "This firmware does not report RX bandwidth. The 24 MHz view is a conservative fallback, not a hardware limit. Updated firmware enables the presets.";
  const outside = experimentalRange(Number(ui.centerFreq.value) - displayWidth / 2, Number(ui.centerFreq.value) + displayWidth / 2);
  ui.liveRegionHint.classList.toggle("experimental", outside);
  ui.liveRegionHint.textContent = outside ? "Amber areas: experimental / uncharacterized · outside 2400–2483.5 MHz" : "Recommended RF band: 2400–2483.5 MHz";
  updateDisplayHint();
  updateCalibration();
  wideband.updateControls();
}

function signed(value) { return `${value >= 0 ? "+" : ""}${value.toFixed(1)}`; }

function populateFrequencyPresets() {
  const wifi = [];
  for (let channel = 1; channel <= 13; channel++) {
    const frequency = 2412 + (channel - 1) * 5;
    wifi.push(`<button type="button" data-frequency="${frequency}" title="Wi-Fi channel ${channel} · ${frequency} MHz">${channel}<small>${frequency}</small></button>`);
  }
  ui.wifiFrequencyPresets.innerHTML = wifi.join("");

}

function closeFrequencyPresets() {
  ui.frequencyPresetMenu.hidden = true;
  ui.frequencyPresetToggle.setAttribute("aria-expanded", "false");
}

function choosePresetFrequency(frequency) {
  const min = Number(ui.centerFreq.min || 100), max = Number(ui.centerFreq.max || 6000);
  const value = Math.max(min, Math.min(max, Number(frequency)));
  ui.centerFreq.value = String(value);
  ui.centerFreq.dispatchEvent(new Event("input", { bubbles: true }));
  closeFrequencyPresets();
  ui.centerFreq.focus();
}

function canApplySpan(s) { return s?.baseline === "ready" && s.lower <= -1 && s.upper >= 1; }

function updateDisplayHint() {
  ui.peakResetButton.hidden = ui.modeSelect.value !== "peak" || ui.viewSelect.value !== "corrected";
  ui.displayHint.textContent = ui.viewSelect.value !== "corrected"
    ? "Raw / DC · processing bypassed"
    : cleanupEnabled ? calibrationStatus?.cleanup?.state === "ready"
      ? "Background reduced; signals may be reduced too. Click Reduce noise to compare."
      : "Measuring background… Keep antenna connected."
    : { average: "Smoothed display", instant: "Latest capture",
      peak: "Strongest signals held", welch: "Smoother noise · less detail" }[ui.modeSelect.value];
}

function setText(element, text) { if (element.textContent !== text) element.textContent = text; }

function updateCalibration() {
  const active = calibrationStep !== "idle", ready = calibrationStatus?.baseline === "ready";
  liveSettings.setCalibrating(active);
  ui.calibrateButton.disabled = preparingLive || active || switching || workspaceMode !== "live"
    || !(state === "connected" || state === "streaming" && worker);
  setText(ui.calibrateButton, ready ? "Recalibrate…" : "Calibrate…");
  ui.cleanButton.disabled = state !== "streaming" || !worker || preparingLive || active;
  ui.cleanButton.setAttribute("aria-pressed", String(cleanupEnabled));
  const clean = calibrationStatus?.cleanup;
  ui.cleanButton.setAttribute("aria-busy", String(cleanupEnabled && state === "streaming" && clean?.state !== "ready"));
  setText(ui.cleanButton.querySelector(".action-label"), cleanupEnabled && state === "streaming" && clean?.state !== "ready"
    ? `Reducing… ${Math.round((clean?.count || 0) / (clean?.target || BASELINE_FRAMES) * 100)}%` : "Reduce noise");
  setText(ui.cleanButton.querySelector(".action-state"), !cleanupEnabled ? "OFF" : clean?.state === "ready" ? "ON" : "…");
  ui.cleanButton.title = cleanupEnabled ? "Click to remove the live reference and compare the original signal."
    : "Measure several captures with the antenna connected, then reduce the measured background. Signals present during measurement may also be reduced.";
  ui.calibrateButton.title = ready ? "Receiver calibration applied. Click to repeat." : "Optional noise-reference calibration with the antenna disconnected.";
  ui.calibrateButton.setAttribute("aria-expanded", String(active));
  ui.calibrationGuide.hidden = !active;
  setText(ui.calibrationLabel, ready ? "Calibrated ✓" : "Calibration · optional");
  let summary = state === "streaming" ? "Flatten the receiver response with a short guided measurement."
    : "Calibration will start the live stream automatically.";
  if (ready) summary = state !== "streaming" ? "Last capture calibrated. A new stream needs a new calibration."
    : ui.viewSelect.value !== "corrected" ? "Calibration saved. Choose Corrected under Display → Signal view to use it."
    : canApplySpan(calibrationStatus) ? "Receiver response corrected · lower-confidence edges marked automatically."
    : "No usable centered band found. Prepare a noise-only input and recalibrate.";
  if (calibrationStep === "collecting") summary = "Measuring the receiver. Keep the antenna disconnected.";
  if (calibrationStep === "reconnect") summary = ready ? "Measurement complete. Reconnect your antenna to view signals."
    : "Calibration reset. Reconnect your antenna before listening.";
  if (calibrationStep === "interrupted") summary = "Calibration interrupted. Reconnect your antenna before listening.";
  setText(ui.calibrationSummary, summary);
  const receiverSummary = `${ui.liveRate.value} MS/s · ${Number(ui.sampleCount.value).toLocaleString()} samples`;
  setText(ui.liveSettingsStatus, active ? summary : receiverSummary + (ready ? " · Calibrated" : ""));
  const content = {
    prepare: ["Disconnect the antenna", "We’ll measure the receiver’s own noise to reduce artifacts. Disconnect the antenna or terminate the input first so real signals are not removed."],
    collecting: ["Measuring receiver noise…", "Keep the antenna disconnected. The correction will be applied automatically."],
    reconnect: ["Reconnect your antenna", !ready ? "The previous calibration no longer applies. Reconnect the antenna to listen, or calibrate again after this step."
      : canApplySpan(calibrationStatus)
      ? "Calibration is applied. Reconnect the antenna to view signals."
      : "The noise reference has no usable centered band. Reconnect the antenna, then remove the calibration or try again with a noise-only input."],
    interrupted: ["Measurement stopped", "This calibration was not completed. Reconnect your antenna to listen, or start a new stream and try again."]
  }[calibrationStep];
  if (content) { setText(ui.calibrationTitle, content[0]); setText(ui.calibrationText, content[1]); }
  ui.calibrationStartButton.hidden = calibrationStep !== "prepare";
  ui.calibrationStartButton.disabled = state !== "streaming" || !worker;
  ui.calibrationDoneButton.hidden = !["reconnect", "interrupted"].includes(calibrationStep);
  ui.calibrationCancelButton.hidden = !["prepare", "collecting"].includes(calibrationStep);
  setText(ui.calibrationCancelButton, calibrationStep === "collecting" ? "Cancel measurement" : "Cancel");
  ui.calibrationProgressWrap.hidden = calibrationStep !== "collecting";
  const target = calibrationStatus?.target || BASELINE_FRAMES * (ui.diversityToggle.checked ? 3 : 1);
  ui.calibrationProgress.max = target;
  ui.calibrationProgress.value = calibrationStatus?.count || 0;
  setText(ui.calibrationProgressText, `${Math.round(ui.calibrationProgress.value / target * 100)}%`);

}

function setDisplayWidth(width) {
  if (displayWidth === width) return;
  displayWidth = width;
  config = readConfig();
  sendProcessing({ config });
}

function clearCalibration() {
  calibrationStatus = null;
  sendProcessing({ clearBaseline: true });
  setDisplayWidth(effectiveDisplayWidth());
  updateControls();
}

function interruptCalibration() {
  if (!["prepare", "collecting"].includes(calibrationStep)) return;
  if (calibrationStep === "collecting") clearCalibration();
  calibrationStep = "interrupted";
}

function cancelPendingCleanup() {
  if (!cleanupEnabled || calibrationStatus?.cleanup?.state === "ready") return;
  cleanupEnabled = false;
  if (calibrationStatus) calibrationStatus.cleanup = { on: false, state: "none" };
  sendOptions();
}

function readOptions() {
  const alpha = Math.min(1, Math.max(0.01, Number(ui.alphaInput.value) || 0.2));
  return { window: ui.windowSelect.value, mode: ui.modeSelect.value, alpha,
    spur: ui.spurToggle.checked, cleanup: cleanupEnabled, diversity: ui.diversityToggle.checked };
}

function sendProcessing(message) {
  if (!worker) return;
  generation++;
  dropped += rawQueue.length;
  rawQueue = [];
  displayQueue = [];
  latest = null;
  autoScale = null;
  worker.postMessage(message);
  plots.reset(config);
  updateControls();
}

function sendOptions() {
  ui.alphaInput.disabled = ui.modeSelect.value !== "average";
  updateDisplayHint();
  sendProcessing({ options: readOptions() });
}

function syncView() {
  plots.showRaw = ui.rawOverlay.checked;
  if (ui.viewSelect.value !== plots.view) plots.setView(ui.viewSelect.value);
}

const DEFAULT_SCALES = { dBFS: [-100, 0], "dBFS/Hz": [-150, -50], "dB above reference": [-5, 35] };
let autoScale = null;

function applyScale(floor, ceiling, unit) {
  if (unit !== plots.unit) plots.setView(plots.view);
  const { scale } = plots;
  if (unit !== plots.unit || Math.abs(floor - scale.floor) > 0.5 || Math.abs(ceiling - scale.ceiling) > 0.5) {
    plots.setScale(floor, ceiling, unit);
  }
  ui.scaleMin.textContent = String(Math.round(floor)).replace("-", "−");
  ui.scaleMax.textContent = `${Math.round(ceiling)} ${unit === "dB above reference" ? "dB" : unit}`;
  ui.spectrumUnit.textContent = unit === "dB above reference" ? "Level above reference · dB" : unit === "dBFS/Hz" ? "Power density · dBFS/Hz" : "Relative level · dBFS";
}

// Adaptive scaling follows the measured noise floor (median) and the strongest signals.
function updateScale() {
  if (!latest) return;
  const view = plots.view, unit = view === "corrected" ? latest.correctedUnit : "dBFS";
  if (ui.autoScale.checked) {
    const values = Array.from(latest[view]).filter(Number.isFinite).sort((a, b) => a - b);
    if (values.length < 8) return;
    const q = p => values[Math.min(values.length - 1, Math.floor((values.length - 1) * p))];
    const noise = q(0.5), high = q(0.995);
    const snap = value => Math.round(value / 5) * 5;
    const target = { floor: snap(noise - 8), ceiling: snap(Math.max(high + 5, noise + 25)) };
    if (!autoScale || autoScale.unit !== unit) autoScale = { ...target, unit };
    else {
      if (Math.abs(target.floor - autoScale.floor) >= 5) autoScale.floor += 0.04 * (target.floor - autoScale.floor);
      if (Math.abs(target.ceiling - autoScale.ceiling) >= 5) autoScale.ceiling += 0.04 * (target.ceiling - autoScale.ceiling);
    }
    applyScale(snap(autoScale.floor), snap(autoScale.ceiling), unit);
  } else {
    if (unit !== plots.unit) [ui.scaleFloor.value, ui.scaleCeiling.value] = DEFAULT_SCALES[unit];
    const floor = Number(ui.scaleFloor.value), ceiling = Number(ui.scaleCeiling.value);
    if (ui.scaleFloor.value !== "" && ui.scaleCeiling.value !== "" && Number.isFinite(floor) && Number.isFinite(ceiling) && floor < ceiling) {
      applyScale(floor, ceiling, unit);
    }
  }
}

function updateAnalysis() {
  const s = latest?.status;
  if (!s) return;
  if (calibrationStep === "collecting" && s.baseline === "none") calibrationStep = "interrupted";
  const completed = calibrationStep === "collecting" && s.baseline === "ready";
  const focusDone = completed && document.activeElement === ui.calibrationCancelButton;
  calibrationStatus = s;
  if (completed) calibrationStep = "reconnect";
  const baseline = s.baseline === "ready" ? "Baseline calibrated ✓"
    : s.baseline === "collecting" ? `Baseline calibrating ${s.count}/${s.target}…` : "Baseline not calibrated";
  const spur = s.spur.on ? s.spur.active ? `Spur rejection ON${s.spur.flagged ? ` (${s.spur.flagged} reference bins)` : ""}` : "Spur rejection awaiting reference or diversity" : "Spur rejection OFF";
  const mode = { instant: "Instant", average: `Average α=${ui.alphaInput.value}`, peak: "Peak hold", welch: "Welch PSD" }[s.mode];
  const window = s.window === "hann" ? "Hann" : "Blackman-Harris";
  ui.processingState.textContent = ["DC corrected ✓", baseline, spur,
    `Tune diversity ${s.diversity ? "ON" : "OFF"}`, s.cleanup.on ? s.cleanup.state === "ready"
      ? `Live-reference cleanup ON (${s.cleanup.flagged} bins)` : `Learning live reference ${s.cleanup.count}/${s.cleanup.target}` : "Live-reference cleanup OFF", mode, window].join(" · ");
  const lines = [];
  if (s.baseline === "ready") {
    lines.push(`Measured reference response ${Math.max(0, s.upper - s.lower).toFixed(1)} MHz (${signed(s.lower)} to ${signed(s.upper)} MHz) within 6 dB of the central median, ${s.frames} reference captures per center`);
    if (!canApplySpan(s)) lines.push("No common centered band of at least 2 MHz: collect a new noise reference");
  }
  if (s.spur.offsets.length) {
    lines.push(`Reference birdies at LO ${s.spur.offsets.map(signed).join(", ")} MHz`);
  }
  if (s.baseline === "collecting") lines.push("Keep the input noise-only until calibration completes");
  if (s.diversity) lines.push("Sequential tuning: use single-center Instant or Peak hold for short bursts");
  if (s.mode === "welch") lines.push(`Welch resolution ${(latest.resolutionHz / 1000).toFixed(2)} kHz/bin · 50% overlap`);
  ui.analysisText.textContent = lines.join(" | ");
  ui.clearBaselineButton.disabled = state !== "streaming" || s.baseline !== "ready" || calibrationStep !== "idle";
  ui.windowSelect.disabled = calibrationStep === "collecting";
  updateCalibration();
  updateDisplayHint();
  if (focusDone) ui.calibrationDoneButton.focus();
}

function updateStats() {
  if (!startedAt) return;
  const seconds = Math.max(0.001, ((endedAt || performance.now()) - startedAt) / 1000);
  ui.streamStats.textContent = `${received.toLocaleString()} received · ${displayed.toLocaleString()} rows · ${(received * config.sampleCount / seconds / 1000).toFixed(1)} kIQ/s over USB · ${dropped} display skips · ${settling} retune skips${latest ? ` · #${latest.sequence} · capture ${latest.captureUs} µs · LO ${latest.centerMHz} MHz` : ""}`;
}

function scheduleRender() {
  if (renderPending) return;
  renderPending = true;
  requestAnimationFrame(() => {
    renderPending = false;
    if (displayQueue.length) {
      const frames = displayQueue.splice(0);
      latest = frames.at(-1);
      syncView();
      for (const frame of frames) {
        if (revision !== frame.revision || plots.unit !== (plots.view === "corrected" ? frame.correctedUnit : "dBFS")) {
          plots.setView(plots.view);
          plots.unit = plots.view === "corrected" ? frame.correctedUnit : "dBFS";
          autoScale = null;
          if (!ui.autoScale.checked) [ui.scaleFloor.value, ui.scaleCeiling.value] = DEFAULT_SCALES[plots.unit];
        }
        revision = frame.revision;
        if (plots.append(frame)) displayed++;
      }
      updateScale();
      updateAnalysis();
      ui.emptyState.hidden = true;
      plots.draw();
    }
    updateStats();
  });
}

function queueFrame(queue, frame) {
  if (queue.length >= QUEUE_LIMIT) {
    // Keep every tuning represented under load, so diversity can complete a cycle.
    const sameCenter = queue.findIndex(item => item.centerHz === frame.centerHz);
    queue.splice(Math.max(0, sameCenter), 1);
    dropped++;
  }
  queue.push(frame);
}

function pumpWorker() {
  if (workerBusy || !worker || !rawQueue.length) return;
  workerBusy = true;
  const frame = rawQueue.shift();
  worker.postMessage({ payload: frame.payload, sequence: frame.sequence, captureUs: frame.captureUs,
    centerMHz: frame.centerHz / 1e6, bandwidth: frame.bandwidth, retuned: frame.retuned, generation }, [frame.payload.buffer]);
}

function initializeWorker() {
  worker?.terminate();
  rawQueue = [];
  displayQueue = [];
  workerBusy = false;
  workerFailure = null;
  generation++;
  revision = null;
  worker = new Worker(new URL("../workers/spectrum-worker.js", import.meta.url), { type: "module" });
  worker.onmessage = ({ data }) => {
    if (data.error) { void handleWorkerError(data.error); return; }
    workerBusy = false;
    if (!data.skipped && data.generation === generation) {
      queueFrame(displayQueue, data);
      scheduleRender();
    }
    pumpWorker();
  };
  worker.onerror = event => { event.preventDefault(); void handleWorkerError(event.message || "FFT worker failed."); };
  worker.postMessage({ options: readOptions() });
  worker.postMessage({ config });
}

async function handleWorkerError(message) {
  if (workerFailure) return;
  workerFailure = message;
  dropped += rawQueue.length + (workerBusy ? 1 : 0);
  rawQueue = [];
  worker?.terminate();
  worker = null;
  workerBusy = false;
  status(message, true);
  await disconnect(true);
}

async function connect() {
  state = "connecting";
  updateControls();
  status("Choose the Bit Pirate SDR CDC port…");
  const pending = new SdrClient({ log });
  try {
    // Keep requestPort directly in the click gesture, before any other await.
    const port = await navigator.serial.requestPort();
    status("Checking SDR adapter…");
    const info = await pending.connect(port);
    client = pending;
    if (!extendedRangeAvailable()) wideExtendedRangeEnabled = false;
    ui.liveRate.value = String(Number(info.fs_hz) / 1e6);
    ui.liveRate.disabled = !client.supportsRate;
    ui.liveGainMode.disabled = !client.supportsGain;
    ui.liveGainMode.value = client.supportsGain ? preferredLiveGainMode : "HARDWARE";
    const gainAtMaximum = ui.liveGainIndex.value === ui.liveGainIndex.max;
    ui.liveGainIndex.max = String(Number(info.gain_max ?? 82));
    if (gainAtMaximum) ui.liveGainIndex.value = ui.liveGainIndex.max;
    displayWidth = effectiveDisplayWidth();
    state = "connected";
    wideband.updatePlan();
    for (const option of ui.sampleCount.options) option.disabled = Number(option.value) > Number(info.max_samples);
    if (Number(ui.sampleCount.value) > Number(info.max_samples)) {
      ui.sampleCount.value = [...ui.sampleCount.options].filter(option => !option.disabled).at(-1).value;
    }
    status("SDR radio ready. Choose a frequency range and start the stream.");
  } catch (error) {
    state = "disconnected";
    if (error.name === "NotFoundError") status("Port selection cancelled.");
    else status(error.message, true);
  }
  updateControls();
}

async function start() {
  if (state !== "connected" || workspaceMode !== "live" || switching) return;
  displayWidth = effectiveDisplayWidth();
  try { config = readConfig(); } catch (error) { status(error.message, true); return; }
  calibrationStatus = null;
  if (!["prepare", "interrupted", "reconnect"].includes(calibrationStep)) calibrationStep = "idle";
  received = displayed = dropped = settling = endedAt = 0;
  latest = null;
  startedAt = performance.now();
  plots.reset(config);
  autoScale = null;
  ui.analysisText.textContent = "";
  ui.processingState.textContent = "DC corrected ✓ · Baseline not calibrated";
  syncView();
  ui.emptyState.hidden = true;
  preparingLive = true;
  state = "streaming";
  updateControls();
  status("Waiting for the first IQ capture…");
  try {
    bandwidthUiKey = null; liveBandwidths = new Map();
    await client.configureRate(Number(ui.liveRate.value));
    await client.configureGain(ui.liveGainMode.value, Number(ui.liveGainIndex.value));
    await client.configureBandwidth(ui.rxExact.value);
    if (state !== "streaming") return;
    displayWidth = effectiveDisplayWidth();
    config = readConfig(); plots.reset(config); updateControls();
    initializeWorker();
    preparingLive = false;
    updateControls();
    await client.stream(config, frame => {
      received++;
      if (received === 1 && state === "streaming") status("Receiving live IQ captures over USB CDC.");
      if (frame.bandwidth) {
        liveBandwidths.set(frame.centerHz, bandwidthMHz(frame.bandwidth));
        const width = Math.min(ui.diversityToggle.checked ? Math.min(...liveBandwidths.values()) : bandwidthMHz(frame.bandwidth), frame.sampleRate / 1e6);
        if (Math.abs(width - displayWidth) > 0.000001) {
          calibrationStatus = null;
          setDisplayWidth(width);
        }
        const key = JSON.stringify(frame.bandwidth);
        if (key !== bandwidthUiKey) { bandwidthUiKey = key; updateControls(); }
      }
      if (frame.retuned) { settling++; return; }
      queueFrame(rawQueue, frame);
      pumpWorker();
    }, ui.diversityToggle.checked ? DIVERSITY_OFFSETS_MHZ : null);
    if (state !== "disconnecting" && !workerFailure) status("Stream stopped. Change settings or start another stream.");
  } catch (error) {
    if (state !== "disconnecting" && !workerFailure) status(error.message, true);
  } finally {
    preparingLive = false;
    endedAt = performance.now();
    if (state !== "disconnecting") {
      state = client?.info ? "connected" : "disconnected";
      if (state === "disconnected") client = null;
    }
    interruptCalibration();
    cancelPendingCleanup();
    updateControls();
    scheduleRender();
  }
}

async function stop() {
  if (state === "scanning") {
    state = "stopping"; updateControls();
    try { await wideband.stop(); } catch (error) { status(error.message, true); await disconnect(true); }
    return;
  }
  if (state !== "streaming") return;
  state = "stopping";
  interruptCalibration();
  cancelPendingCleanup();
  updateControls();
  status("Stopping after the current block…");
  try { await client.stop(); }
  catch (error) { status(error.message, true); await disconnect(true); }
}

async function disconnect(preserveMessage = false) {
  if (state === "disconnecting") return;
  state = "disconnecting";
  interruptCalibration();
  cancelPendingCleanup();
  updateControls();
  if (!preserveMessage) status("Stopping the stream and releasing the CDC port…");
  try { await client?.disconnect(); }
  catch (error) { status(error.message, true); preserveMessage = true; }
  client = null;
  state = "disconnected";
  if (!preserveMessage) status("Disconnected. The last capture remains visible.");
  updateControls();
}

async function switchWorkspace(next, frequency = null) {
  if (switching || next === workspaceMode && frequency === null) return;
  if (calibrationStep !== "idle" && workspaceMode === "live" || wideband.phase !== "idle" && workspaceMode === "wide") return;
  switching = true;
  updateControls();
  try {
    if (wideband.active) await wideband.stop();
    else if (state === "streaming") { const task = client.streamTask; await stop(); await task; }
    // Old live results must not leak into a new acquisition after switching tabs.
    worker?.terminate(); worker = null; workerBusy = false; generation++; rawQueue = []; displayQueue = [];
    workspaceMode = next;
    ui.livePanel.hidden = next !== "live"; ui.widePanel.hidden = next !== "wide";
    for (const [tab, mode] of [[ui.wideTab, "wide"], [ui.liveTab, "live"]]) {
      const selected = next === mode;
      tab.setAttribute("aria-selected", String(selected)); tab.tabIndex = selected ? 0 : -1;
      tab.classList.toggle("is-active", selected);
    }
    if (frequency !== null) {
      ui.diversityToggle.checked = false;
      ui.rxPreset.value = ui.rxExact.value = "40";
      const [min, max] = liveLimits();
      ui.centerFreq.value = Math.max(min, Math.min(max, Math.round(frequency)));
      ui.viewSelect.value = "corrected"; ui.modeSelect.value = "average"; ui.autoScale.checked = true;
      ui.alphaInput.disabled = false; ui.scaleFloor.disabled = ui.scaleCeiling.disabled = true;
      displayWidth = effectiveDisplayWidth(); calibrationStatus = null;
    }
    if (next === "wide") wideband.updatePlan();
  } catch (error) { status(error.message, true); }
  finally { switching = false; updateControls(); }
  (workspaceMode === "live" ? ui.liveTab : ui.wideTab).focus();
  if (frequency !== null && state === "connected" && workspaceMode === "live") void start();
}

ui.rxPreset.addEventListener("change", () => {
  if (ui.rxPreset.value !== "custom") ui.rxExact.value = ui.rxPreset.value;
  updateControls();
});
ui.rxExact.addEventListener("input", () => {
  const value = ui.rxExact.value.trim().toUpperCase();
  ui.rxPreset.value = ["13", "40", "WIDE"].includes(value) ? value : "custom";
  updateControls();
});

ui.liveRate.addEventListener("change", () => {
  displayWidth = effectiveDisplayWidth();
  calibrationStatus = null; autoScale = null; updateControls();
});
ui.liveGainMode.addEventListener("change", () => {
  preferredLiveGainMode = ui.liveGainMode.value;
  ui.liveGainIndex.disabled = ui.liveGainMode.value !== "MANUAL";
  updateControls();
});
ui.liveGainIndex.addEventListener("input", updateControls);

ui.liveTab.addEventListener("click", () => void switchWorkspace("live"));
ui.wideTab.addEventListener("click", () => void switchWorkspace("wide"));
for (const tab of [ui.wideTab, ui.liveTab]) tab.addEventListener("keydown", event => {
  if (!["ArrowLeft", "ArrowRight", "Home", "End"].includes(event.key)) return;
  event.preventDefault();
  const next = event.key === "Home" ? "wide" : event.key === "End" ? "live" : workspaceMode === "live" ? "wide" : "live";
  void switchWorkspace(next);
});

ui.connectButton.addEventListener("click", connect);
ui.disconnectButton.addEventListener("click", () => void disconnect());
ui.startButton.addEventListener("click", start);
ui.stopButton.addEventListener("click", stop);
ui.viewSelect.addEventListener("change", () => { autoScale = null; syncView(); updateScale(); plots.draw(); updateDisplayHint(); updateCalibration(); });
ui.rawOverlay.addEventListener("change", () => { syncView(); plots.draw(); });
ui.liveAggregation.addEventListener("change", () => { plots.waterfallAggregation = ui.liveAggregation.value; });
for (const control of [ui.modeSelect, ui.alphaInput, ui.windowSelect, ui.spurToggle, ui.diversityToggle]) {
  control.addEventListener("change", () => {
    if ([ui.windowSelect, ui.diversityToggle].includes(control)) {
      calibrationStatus = null;
      setDisplayWidth(effectiveDisplayWidth());
    }
    sendOptions();
    updateControls();
  });
}
ui.autoScale.addEventListener("change", () => {
  ui.scaleFloor.disabled = ui.scaleCeiling.disabled = ui.autoScale.checked;
  if (!ui.autoScale.checked) {
    ui.scaleFloor.value = Math.round(plots.scale.floor);
    ui.scaleCeiling.value = Math.round(plots.scale.ceiling);
  } else autoScale = null;
  updateScale();
});
for (const input of [ui.scaleFloor, ui.scaleCeiling]) input.addEventListener("input", updateScale);
ui.resetButton.addEventListener("click", () => sendProcessing({ reset: true }));
ui.peakResetButton.addEventListener("click", () => sendProcessing({ reset: true }));
ui.clearBaselineButton.addEventListener("click", clearCalibration);
ui.cleanButton.addEventListener("click", () => {
  if (state !== "streaming" || !worker || preparingLive || calibrationStep !== "idle") return;
  cleanupEnabled = !cleanupEnabled;
  if (calibrationStatus) calibrationStatus.cleanup = { state: "collecting", count: 0, target: BASELINE_FRAMES * (ui.diversityToggle.checked ? 3 : 1) };
  if (cleanupEnabled) { ui.viewSelect.value = "corrected"; ui.rawOverlay.checked = false; syncView(); }
  sendOptions();
});
ui.calibrateButton.addEventListener("click", () => {
  if (!client?.info || !["connected", "streaming"].includes(state) || preparingLive || calibrationStep !== "idle") return;
  ui.advancedSettings.open = true;
  liveSettings.select("calibration");
  if (state !== "streaming") void start();
  calibrationStep = "prepare";
  updateControls();
  if (!ui.calibrationStartButton.disabled) ui.calibrationStartButton.focus();
});
ui.calibrationStartButton.addEventListener("click", () => {
  if (state !== "streaming" || !worker || calibrationStep !== "prepare") return;
  calibrationStep = "collecting";
  cleanupEnabled = false;
  calibrationStatus = null;
  ui.viewSelect.value = "corrected";
  ui.spurToggle.checked = true;
  sendOptions();
  syncView();
  setDisplayWidth(effectiveDisplayWidth());
  sendProcessing({ baseline: BASELINE_FRAMES });
  ui.calibrationCancelButton.focus();
});
ui.calibrationCancelButton.addEventListener("click", () => {
  if (calibrationStep === "collecting") {
    calibrationStep = "interrupted";
    clearCalibration();
    ui.calibrationDoneButton.focus();
  } else {
    calibrationStep = "idle";
    updateControls();
    ui.calibrateButton.focus();
  }
});
ui.calibrationDoneButton.addEventListener("click", () => {
  calibrationStep = "idle";
  updateControls();
  if (!ui.calibrateButton.disabled) ui.calibrateButton.focus();
});
for (const input of [ui.centerFreq, ui.sampleCount]) input.addEventListener("input", () => {
  calibrationStatus = null;
  displayWidth = bandwidthMHz(client?.bandwidth);
  updateControls();
});

function setExtendedRange(enabled) {
  wideExtendedRangeEnabled = Boolean(enabled) && extendedRangeAvailable();
  ui.scanExtendedRange.checked = wideExtendedRangeEnabled;
  wideband.rangeModeChanged();
  updateControls();
}

ui.scanExtendedRange.addEventListener("change", () => setExtendedRange(ui.scanExtendedRange.checked));

populateFrequencyPresets();

ui.frequencyPresetToggle.addEventListener("click", event => {
  event.stopPropagation();
  if (ui.frequencyPresetToggle.disabled) return;
  const opening = ui.frequencyPresetMenu.hidden;
  ui.frequencyPresetMenu.hidden = !opening;
  ui.frequencyPresetToggle.setAttribute("aria-expanded", String(opening));
});

ui.frequencyPresetMenu.addEventListener("click", event => {
  const button = event.target.closest("[data-frequency]");
  if (!button) return;
  choosePresetFrequency(button.dataset.frequency);
});

document.addEventListener("pointerdown", event => {
  if (!ui.frequencyPresetMenu.hidden && !event.target.closest(".frequency-input-wrap")) closeFrequencyPresets();
});

document.addEventListener("keydown", event => {
  if (event.key === "Escape" && !ui.frequencyPresetMenu.hidden) {
    closeFrequencyPresets();
    ui.frequencyPresetToggle.focus();
  }
});

if (supported) {
  navigator.serial.addEventListener("disconnect", event => {
    if (event.target === client?.transport.port) {
      status("The CDC device was unplugged. Reconnect it to start another stream.", true);
      void disconnect(true);
    }
  });
}
window.addEventListener("pagehide", () => {
  worker?.terminate();
  // Best effort on navigation; the firmware also leaves streaming on USB write timeout.
  void disconnect();
});
ui.serialUnsupported.hidden = supported;
updateControls();
