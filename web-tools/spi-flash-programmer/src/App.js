import { downloadBytes } from "../../shared/files/download.js";
import { RflasherAdapter, formatBytes } from "./RflasherAdapter.js?v=20260912";

const elements = {
  serialUnsupported: document.querySelector("#serialUnsupported"),
  connectButton: document.querySelector("#connectButton"),
  disconnectButton: document.querySelector("#disconnectButton"),
  probeButton: document.querySelector("#probeButton"),
  readButton: document.querySelector("#readButton"),
  downloadButton: document.querySelector("#downloadButton"),
  dumpButton: document.querySelector("#dumpButton"),
  downloadDumpButton: document.querySelector("#downloadDumpButton"),
  writeFile: document.querySelector("#writeFile"),
  writeFileName: document.querySelector("#writeFileName"),
  writeButton: document.querySelector("#writeButton"),
  clearLogButton: document.querySelector("#clearLogButton"),
  connectionStatus: document.querySelector("#connectionStatus"),
  programmerName: document.querySelector("#programmerName"),
  ifaceVersion: document.querySelector("#ifaceVersion"),
  busType: document.querySelector("#busType"),
  serialBuffer: document.querySelector("#serialBuffer"),
  maxRead: document.querySelector("#maxRead"),
  maxWrite: document.querySelector("#maxWrite"),
  spiFrequencySelect: document.querySelector("#spiFrequencySelect"),
  spiFrequencyCustom: document.querySelector("#spiFrequencyCustom"),
  spiFrequencyActual: document.querySelector("#spiFrequencyActual"),
  jedecId: document.querySelector("#jedecId"),
  chipName: document.querySelector("#chipName"),
  chipCapacity: document.querySelector("#chipCapacity"),
  legacyId: document.querySelector("#legacyId"),
  sfdpSignature: document.querySelector("#sfdpSignature"),
  readStart: document.querySelector("#readStart"),
  readLength: document.querySelector("#readLength"),
  hexPreview: document.querySelector("#hexPreview"),
  progressLabel: document.querySelector("#progressLabel"),
  operationProgress: document.querySelector("#operationProgress"),
  progressValue: document.querySelector("#progressValue"),
  logOutput: document.querySelector("#logOutput"),
};
const operationTabs = Array.from(document.querySelectorAll("[data-operation-tab]"));
const operationPanels = Array.from(document.querySelectorAll("[data-operation-panel]"));
const PREVIEW_MAX_CHARS = 220000;

let adapter = null;
let busy = false;
let connected = false;
let lastRead = null;
let fullDump = null;
let lastProbe = null;

function init() {
  setConnectionButtons(false);
  if (!("serial" in navigator)) {
    elements.serialUnsupported.hidden = false;
    elements.connectButton.disabled = true;
    setStatus("Unsupported browser");
    return;
  }

  adapter = new RflasherAdapter({ log });
  elements.connectButton.addEventListener("click", connect);
  elements.disconnectButton.addEventListener("click", disconnect);
  elements.probeButton.addEventListener("click", probe);
  elements.readButton.addEventListener("click", readRange);
  elements.downloadButton.addEventListener("click", downloadLastRead);
  elements.dumpButton.addEventListener("click", dumpFullChip);
  elements.downloadDumpButton.addEventListener("click", downloadFullDump);
  elements.writeButton.addEventListener("click", writeFullChip);
  elements.writeFile.addEventListener("change", () => {
    renderWriteFileName();
    renderControls();
  });
  elements.spiFrequencySelect.addEventListener("change", handleSpiFrequencySelectChange);
  elements.spiFrequencyCustom.addEventListener("change", changeSpiFrequency);
  for (const tab of operationTabs) {
    tab.addEventListener("click", () => selectOperationTab(tab.dataset.operationTab));
  }
  elements.clearLogButton.addEventListener("click", () => {
    elements.logOutput.textContent = "";
  });
  log("Loaded. Connect an ESP32 Bit Pirate in serprog adapter mode or another compatible device.");
}

function selectOperationTab(name) {
  for (const tab of operationTabs) {
    const isActive = tab.dataset.operationTab === name;
    tab.classList.toggle("is-active", isActive);
    tab.setAttribute("aria-selected", String(isActive));
  }

  for (const panel of operationPanels) {
    const isActive = panel.dataset.operationPanel === name;
    panel.classList.toggle("is-active", isActive);
    panel.hidden = !isActive;
  }
}

async function connect() {
  await runOperation("Connecting", async () => {
    const info = await adapter.connect({ spiFrequency: readSelectedSpiFrequency() });
    renderProgrammerInfo(info);
    connected = true;
    setStatus("Connected");
    setConnectionButtons(true);
    renderControls();

    if (info.programmerName) {
      log(`Connected serprog adapter: ${info.programmerName}.`);
    }
  });
}

async function handleSpiFrequencySelectChange() {
  renderControls();
  if (elements.spiFrequencySelect.value === "custom") {
    elements.spiFrequencyCustom.focus();
    elements.spiFrequencyCustom.select();
    return;
  }
  await changeSpiFrequency();
}

async function changeSpiFrequency() {
  if (!connected || busy) {
    renderControls();
    return;
  }

  await runOperation("Setting SPI speed", async () => {
    const actual = await adapter.setSpiFrequency(readSelectedSpiFrequency());
    syncSpiFrequencySelect(actual);
  });
}

async function disconnect() {
  await runOperation("Disconnecting", async () => {
    await adapter.disconnect();
    connected = false;
    setStatus("Disconnected");
    resetProgrammerInfo();
    resetProbeInfo();
    setConnectionButtons(false);
    renderControls();
  });
}

async function probe() {
  await runOperation("Probing", async () => {
    lastProbe = await adapter.probe();
    fullDump = null;
    renderProbe(lastProbe);
    renderControls();
  });
}

async function readRange() {
  await runOperation("Reading", async () => {
    const start = parseNumber(elements.readStart.value);
    const length = parseNumber(elements.readLength.value);
    fullDump = null;
    resetPreview(`Reading ${length.toLocaleString()} bytes from 0x${start.toString(16).padStart(6, "0")}...\n\n`);
    resetProgress("Reading", 0);
    lastRead = await adapter.readRange(start, length, {
      onProgress: renderProgress,
      onChunk: ({ offset, bytes }) => appendPreview(`${hexdump(bytes, offset)}\n`),
    });
    renderProgress({ done: lastRead.length, total: lastRead.length, phase: "Complete" });
    renderControls();
    log(`Read completed: ${lastRead.length} bytes.`);
  });
}

async function dumpFullChip() {
  await runOperation("Dumping", async () => {
    await ensureIdentifiedChip();
    lastRead = null;
    fullDump = null;
    resetPreview("Dumping full chip...\n\n");
    renderControls();
    resetProgress("Reading", 0);
    fullDump = await adapter.dumpFull({
      onProgress: (progress) => {
        renderProgress(progress);
      },
      onChunk: ({ offset, bytes }) => appendPreview(`${hexdump(bytes, offset)}\n`),
    });
    renderProgress({ done: fullDump.length, total: fullDump.length, phase: "Complete" });
    renderControls();
    log(`Full dump completed: ${fullDump.length} bytes.`);
  });
}

async function writeFullChip() {
  await runOperation("Writing", async () => {
    const file = elements.writeFile.files[0];
    if (!file) {
      throw new Error("Select a binary file first.");
    }

    await ensureIdentifiedChip();
    const chip = requireIdentifiedChip();
    if (file.size !== chip.capacity) {
      throw new Error(`Selected file must be exactly ${chip.capacity} bytes for ${chip.model}; got ${file.size} bytes.`);
    }

    const confirmed = window.confirm(
      `Write ${file.name} to ${chip.manufacturer} ${chip.model}?\n\nThis will erase the entire chip, program ${file.size} bytes, and verify the result.`
    );
    if (!confirmed) {
      throw new Error("Write cancelled by user.");
    }

    resetProgress("Loading file", 0);
    const bytes = new Uint8Array(await file.arrayBuffer());
    // reset the preview to show writing message
    resetPreview(`Writing ${bytes.length.toLocaleString()} bytes to ${chip.manufacturer} ${chip.model}...\n\n`);
    await adapter.writeFull(bytes, {
      onProgress: renderProgress,
    });
    renderProgress({ done: bytes.length, total: bytes.length, phase: "Complete" });
  });
}

function downloadLastRead() {
  if (!lastRead) {
    return;
  }

  const chipPart = lastProbe?.chip?.model ? lastProbe.chip.model.replace(/[^a-z0-9]+/gi, "-") : "spi-flash";
  const stamp = new Date().toISOString().replace(/[-:]/g, "").slice(0, 15);
  downloadBytes(lastRead, `${chipPart}-test-read-${stamp}.bin`);
}

function downloadFullDump() {
  if (!fullDump) {
    return;
  }

  const chipPart = lastProbe?.chip?.model ? lastProbe.chip.model.replace(/[^a-z0-9]+/gi, "-") : "spi-flash";
  const stamp = new Date().toISOString().replace(/[-:]/g, "").slice(0, 15);
  downloadBytes(fullDump, `${chipPart}-full-dump-${stamp}.bin`);
}

async function runOperation(label, task) {
  if (busy) {
    log("Operation already running.");
    return;
  }

  busy = true;
  setButtonsDuringBusy(true);
  setStatus(label);

  try {
    await task();
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    setStatus("Error");
    log(`ERROR: ${message}`);
  } finally {
    busy = false;
    setButtonsDuringBusy(false);
    renderControls();
  }
}

function setButtonsDuringBusy(isBusy) {
  for (const button of [
    elements.connectButton,
    elements.disconnectButton,
    elements.probeButton,
    elements.readButton,
    elements.downloadButton,
    elements.dumpButton,
    elements.downloadDumpButton,
    elements.writeButton,
  ]) {
    button.classList.toggle("is-busy", isBusy);
    button.disabled = isBusy;
  }
  elements.spiFrequencySelect.disabled = isBusy;
  elements.spiFrequencyCustom.disabled = isBusy;
}

function setConnectionButtons(connected) {
  elements.connectButton.hidden = connected;
  elements.disconnectButton.hidden = !connected;
  elements.connectButton.disabled = connected;
  elements.disconnectButton.disabled = !connected;
}

function renderProgrammerInfo(info) {
  elements.programmerName.textContent = info.programmerName || "-";
  elements.ifaceVersion.textContent = String(info.iface);
  elements.busType.textContent = (info.busType & 0x08) ? "SPI" : `0x${info.busType.toString(16)}`;
  elements.serialBuffer.textContent = `${info.serialBuffer} bytes`;
  elements.maxRead.textContent = `${info.maxRead} bytes`;
  elements.maxWrite.textContent = `${info.maxWrite} bytes`;
  syncSpiFrequencySelect(info.spiFrequency);
}

function readSelectedSpiFrequency() {
  const value = elements.spiFrequencySelect.value === "custom"
    ? Math.round(Number.parseFloat(elements.spiFrequencyCustom.value) * 1000000)
    : Number.parseInt(elements.spiFrequencySelect.value, 10);
  if (!Number.isInteger(value) || value <= 0 || value > 0xffffffff) {
    throw new Error("SPI frequency must be greater than 0 MHz and within the 32-bit serprog limit.");
  }
  return value;
}

function syncSpiFrequencySelect(actualFrequency) {
  const option = Array.from(elements.spiFrequencySelect.options)
    .find((item) => Number.parseInt(item.value, 10) === actualFrequency);
  if (option) {
    elements.spiFrequencySelect.value = option.value;
  } else if (Number.isInteger(actualFrequency) && actualFrequency > 0) {
    elements.spiFrequencySelect.value = "custom";
    elements.spiFrequencyCustom.value = formatFrequencyMhzInput(actualFrequency);
  }
  elements.spiFrequencyCustom.hidden = elements.spiFrequencySelect.value !== "custom";
  elements.spiFrequencyActual.textContent = Number.isInteger(actualFrequency) && actualFrequency > 0
    ? `Actual: ${formatFrequency(actualFrequency)}`
    : "Actual: -";
}

function resetProgrammerInfo() {
  for (const item of [
    elements.programmerName,
    elements.ifaceVersion,
    elements.busType,
    elements.serialBuffer,
    elements.maxRead,
    elements.maxWrite,
  ]) {
    item.textContent = "-";
  }
}

function renderProbe(result) {
  elements.jedecId.textContent = formatBytes(result.jedecId).toUpperCase();
  elements.legacyId.textContent = formatBytes(result.legacyId).toUpperCase();
  elements.sfdpSignature.textContent = formatBytes(result.sfdpHeader.slice(0, 4)).toUpperCase();

  if (result.chip) {
    elements.chipName.textContent = `${result.chip.manufacturer} ${result.chip.model}`;
    elements.chipCapacity.textContent = formatCapacity(result.chip.capacity);
  } else {
    elements.chipName.textContent = "Not in compatible chip database";
    elements.chipCapacity.textContent = "-";
  }
}

function renderControls() {
  const chip = lastProbe?.chip;
  const hasChip = Boolean(chip?.capacity);
  const writeFile = elements.writeFile.files[0];
  const hasWriteFile = Boolean(writeFile);
  const writeFileMatchesChip = Boolean(hasChip && writeFile && writeFile.size === chip.capacity);
  const canStartWrite = hasWriteFile && (!hasChip || writeFileMatchesChip);

  elements.probeButton.disabled = !connected || busy;
  elements.readButton.disabled = !connected || busy;
  elements.downloadButton.disabled = !lastRead || busy;
  elements.dumpButton.disabled = !connected || busy;
  elements.dumpButton.title = hasChip ? "" : "Probe will run automatically before dumping.";
  elements.downloadDumpButton.disabled = !fullDump || busy;
  elements.downloadDumpButton.title = fullDump ? "" : "Run Dump full chip first.";
  elements.writeButton.disabled = !connected || !canStartWrite || busy;
  if (!hasWriteFile) {
    elements.writeButton.title = "Select a binary file first.";
  } else if (!hasChip) {
    elements.writeButton.title = "Probe will run automatically before writing.";
  } else if (!writeFileMatchesChip) {
    elements.writeButton.title = `Selected file must be exactly ${chip.capacity} bytes for ${chip.model}.`;
  } else {
    elements.writeButton.title = "";
  }
  elements.spiFrequencySelect.disabled = busy;
  elements.spiFrequencyCustom.disabled = busy;
  elements.spiFrequencyCustom.hidden = elements.spiFrequencySelect.value !== "custom";
}

function renderWriteFileName() {
  const file = elements.writeFile.files[0];
  if (!file) {
    elements.writeFileName.textContent = "No file selected";
    elements.writeFileName.title = "";
    return;
  }

  elements.writeFileName.textContent = `${file.name} (${formatCapacity(file.size)})`;
  elements.writeFileName.title = `${file.name} - ${file.size} bytes`;
}

function requireIdentifiedChip() {
  const chip = lastProbe?.chip;
  if (!chip?.capacity) {
    throw new Error("Probe and identify a supported SPI flash chip first.");
  }
  return chip;
}

async function ensureIdentifiedChip() {
  if (lastProbe?.chip?.capacity) {
    return lastProbe.chip;
  }

  log("No identified chip cached. Probing before full-chip operation.");
  lastProbe = await adapter.probe();
  renderProbe(lastProbe);
  return requireIdentifiedChip();
}

function resetProbeInfo() {
  for (const item of [
    elements.jedecId,
    elements.chipName,
    elements.chipCapacity,
    elements.legacyId,
    elements.sfdpSignature,
  ]) {
    item.textContent = "-";
  }
  lastProbe = null;
  lastRead = null;
  fullDump = null;
  elements.hexPreview.textContent = "No data read yet.";
  resetProgress("Idle", 0);
}

function parseNumber(value) {
  const trimmed = value.trim().toLowerCase();
  if (/^0x[0-9a-f]+$/.test(trimmed)) {
    return Number.parseInt(trimmed, 16);
  }
  if (/^[0-9]+$/.test(trimmed)) {
    return Number.parseInt(trimmed, 10);
  }
  throw new Error(`Invalid number: ${value}`);
}

function hexdump(bytes, startAddress = 0) {
  const lines = [];
  for (let offset = 0; offset < bytes.length; offset += 16) {
    const chunk = bytes.slice(offset, offset + 16);
    const address = (startAddress + offset).toString(16).padStart(8, "0");
    const hex = Array.from(chunk, (byte) => byte.toString(16).padStart(2, "0")).join(" ").padEnd(47, " ");
    const ascii = Array.from(chunk, (byte) => byte >= 0x20 && byte <= 0x7e ? String.fromCharCode(byte) : ".").join("");
    lines.push(`${address}  ${hex}  |${ascii}|`);
  }
  return lines.join("\n");
}

function resetPreview(text = "") {
  elements.hexPreview.textContent = text;
  scrollPreviewToBottom();
}

function appendPreview(text) {
  const current = elements.hexPreview.textContent;
  let next = `${current}${text}`;
  if (next.length > PREVIEW_MAX_CHARS) {
    const overflow = next.length - PREVIEW_MAX_CHARS;
    const trimAt = next.indexOf("\n", overflow);
    next = `[preview truncated; Save range/full dump keeps all bytes]\n${next.slice(trimAt > -1 ? trimAt + 1 : overflow)}`;
  }
  elements.hexPreview.textContent = next;
  scrollPreviewToBottom();
}

function scrollPreviewToBottom() {
  elements.hexPreview.scrollTop = elements.hexPreview.scrollHeight;
}

function formatCapacity(bytes) {
  if (bytes >= 1024 * 1024) {
    return `${bytes / (1024 * 1024)} MiB`;
  }
  return `${bytes / 1024} KiB`;
}

function formatFrequency(hz) {
  if (hz >= 1000000) {
    return `${hz / 1000000} MHz`;
  }
  if (hz >= 1000) {
    return `${hz / 1000} kHz`;
  }
  return `${hz} Hz`;
}

function formatFrequencyMhzInput(hz) {
  return Number((hz / 1000000).toFixed(6)).toString();
}

function renderProgress({ done, total, phase }) {
  const percent = total > 0 ? Math.floor((done / total) * 100) : 0;
  elements.progressLabel.textContent = phase || "Working";
  elements.operationProgress.value = Math.max(0, Math.min(100, percent));
  elements.progressValue.textContent = `${Math.max(0, Math.min(100, percent))}%`;
}

function resetProgress(label, percent) {
  elements.progressLabel.textContent = label;
  elements.operationProgress.value = percent;
  elements.progressValue.textContent = `${percent}%`;
}

function log(message) {
  const line = `[${new Date().toLocaleTimeString()}] ${message}`;
  elements.logOutput.textContent += `${line}\n`;
  elements.logOutput.scrollTop = elements.logOutput.scrollHeight;
}

function setStatus(status) {
  elements.connectionStatus.textContent = status;
}

init();
