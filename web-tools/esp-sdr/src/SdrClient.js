// SPDX-License-Identifier: MIT
import { SdrSerialTransport } from "./SdrSerialTransport.js";
import { HEADER_BYTES, SAMPLE_COUNTS, SAMPLE_FORMAT, bandwidthSetting, parseBandwidth, parseRate, parseGain, adapterError,
  decodeIqPayload, headerBytesForVersion, parseHeader, parseInfo, validateConfig, validateIqFrame } from "./SdrProtocol.js";

export class SdrClient {
  constructor({ transport = new SdrSerialTransport(), log = () => {} } = {}) {
    this.transport = transport;
    this.log = log;
    this.info = null;
    this.streaming = false;
    this.disconnecting = false;
    this.streamTask = null;
    this.stopTask = null;
    this.stopDeadline = Infinity;
  }

  async connect(port) {
    this.disconnecting = false;
    try {
      await this.transport.open(port);
      await this.transport.write("\nINFO\n");
      const deadline = performance.now() + 5000;
      let infoLine = null;
      while (true) {
        const line = await this.transport.readLine(deadline);
        if (line) this.log(line);
        if (line.startsWith("ERR ")) throw new Error(line);
        if (line.startsWith("INFO BPRF1 ")) infoLine = line;
        if (line === "END" && infoLine) {
          this.info = parseInfo(infoLine);
          if (this.info.bw_control === "1") await this.configureBandwidth();
          return this.info;
        }
      }
    } catch (error) {
      await this.transport.close();
      throw error;
    }
  }

  get supportsBandwidth() { return this.info?.bw_control === "1"; }
  get version2() { return this.info?.frame_versions?.split(",").includes("2") ?? false; }
  get supportsRate() { return Boolean(this.info?.rates_hz); }
  get supportsGain() { return this.info?.gain_control === "1"; }
  get supportsIq10Packed() { return this.info?.sample_formats?.split(",").includes("IQ10_PACKED") ?? false; }

  async configureRate(msps) {
    if (!this.supportsRate) {
      if (Number(msps) !== Number(this.info?.fs_hz) / 1e6) throw new Error("This firmware does not support changing the SDR sample rate.");
      return { fsHz: Number(this.info.fs_hz) };
    }
    if (this.streaming || this.configuring || this.disconnecting) throw new Error("Stop before changing the sample rate.");
    this.configuring = true;
    try {
      await this.transport.write(`RATE ${Number(msps)}\n`);
      const deadline = performance.now() + 5000; let result = null;
      while (true) {
        const line = await this.transport.readLine(deadline);
        if (line) this.log(line);
        if (line.startsWith("ERR ")) throw new Error(line);
        if (line.startsWith("RATE ")) result = parseRate(line);
        if (line === "END" && result) { this.info.fs_hz = String(result.fsHz); return result; }
      }
    } finally { this.configuring = false; }
  }

  async configureGain(mode, index = 0) {
    if (!this.supportsGain) {
      if (String(mode).toUpperCase() !== "HARDWARE") throw new Error("Manual gain is unavailable in this firmware.");
      return { supported: false, mode: "HARDWARE", index: -1, max: 0 };
    }
    if (this.streaming || this.configuring || this.disconnecting) throw new Error("Stop before changing RX gain.");
    const selected = String(mode).toUpperCase();
    const command = selected === "MANUAL" ? `GAIN MANUAL ${Number(index)}` : "GAIN HARDWARE";
    this.configuring = true;
    try {
      await this.transport.write(`${command}\n`);
      const deadline = performance.now() + 5000; let result = null;
      while (true) {
        const line = await this.transport.readLine(deadline);
        if (line) this.log(line);
        if (line.startsWith("ERR ")) throw new Error(line);
        if (line.startsWith("GAIN ")) result = parseGain(line);
        if (line === "END" && result) { this.gain = result; return result; }
      }
    } finally { this.configuring = false; }
  }

  async configureBandwidth(value = null) {
    if (!this.supportsBandwidth) return null;
    if (this.streaming || this.configuring || this.disconnecting) throw new Error("Stop before changing RX bandwidth.");
    this.configuring = true;
    try {
      const setting = value === null ? null : bandwidthSetting(value, this.info);
      const command = setting === null ? "BANDWIDTH?" : `BANDWIDTH ${setting}`;
      this.log(command);
      await this.transport.write(`${command}\n`);
      const deadline = performance.now() + 5000;
      let result = null;
      while (true) {
        const line = await this.transport.readLine(deadline);
        if (line) this.log(line);
        if (line.startsWith("ERR ")) throw new Error(line);
        if (line.startsWith("BANDWIDTH ")) result = parseBandwidth(line);
        if (line === "END" && result) {
          // The idle reply may contain the PREVIOUS capture's readback. A set only
          // supplies a target until the next IQ frame acknowledges the actual filter.
          if (setting !== null) result.estimatedHz = 0;
          this.bandwidth = result;
          return result;
        }
      }
    } catch (error) {
      await this.transport.close(); this.info = null;
      throw error;
    } finally { this.configuring = false; }
  }

  async readFrame(deadline) {
    const header = await this.transport.readExact(HEADER_BYTES, deadline);
    const headerBytes = headerBytesForVersion(header[4]);
    if (!headerBytes) return parseHeader(header);
    if (headerBytes === HEADER_BYTES) return parseHeader(header);
    const extended = new Uint8Array(headerBytes);
    extended.set(header); extended.set(await this.transport.readExact(headerBytes - HEADER_BYTES, deadline), HEADER_BYTES);
    return parseHeader(extended);
  }

  // A short stopped stream gives AUTO and quantized filters a fresh readback
  // before planning a sweep. This uses the same END/error handling as acquisition.
  async measureBandwidth(centerMHz) {
    if (!this.version2) return;
    await this.startStream({ sampleCount: 256, sampleRate: Number(this.info.fs_hz) }, [centerMHz], frame => {
      if (!frame.retuned) void this.stop();
    });
  }

  stream(config, onFrame, offsetsMHz = null) {
    config = validateConfig(config.startMHz, config.endMHz, config.sampleCount, config.sampleRate ?? Number(this.info.fs_hz));
    const centers = (offsetsMHz ?? [0]).map(offset => config.centerMHz + offset);
    return this.startStream(config, centers, onFrame);
  }

  sweep(plan, onFrame) {
    return this.startStream({ sampleCount: plan.sampleCount, sampleRate: plan.sampleRate }, plan.centers, onFrame, plan.dwellFrames, true);
  }

  startStream(config, centers, onFrame, dwellFrames = 4, sweeping = false) {
    if (!this.info) throw new Error("Connect to the SDR adapter first.");
    if (this.streaming || this.configuring || this.disconnecting) throw new Error("A stream is already running.");
    if (!SAMPLE_COUNTS.includes(config.sampleCount)) throw new Error("Choose a supported sample count.");
    if (!Number.isInteger(dwellFrames) || dwellFrames < 1 || dwellFrames > 64) throw new Error("Invalid sweep dwell.");
    if (config.sampleCount > Number(this.info.max_samples)) throw new Error("The adapter does not support this sample count.");
    if (!centers.length || new Set(centers).size !== centers.length) throw new Error("Choose distinct tuning centers.");
    for (const center of centers) {
      if (!Number.isInteger(center) || center < Number(this.info.center_min_mhz) || center > Number(this.info.center_max_mhz)) {
        throw new Error("The center frequency is outside this adapter's tuning range.");
      }
    }
    this.streaming = true;
    this.stopTask = null;
    this.stopDeadline = Infinity;
    this.stopRequested = false;
    this.streamTask = this.readStream(config, centers, onFrame, dwellFrames, sweeping);
    return this.streamTask;
  }

  // TUNE has no text acknowledgement: the frame's own center is authoritative.
  // Keep reading buffered captures at the previous center until the requested one arrives.
  async readStream(config, centers, onFrame, dwellFrames, sweeping) {
    try {
      let index = 0, activeCenter = centers[0], pendingCenter = null, dwell = 0;
      let sweepId = 0;
      let tuneDeadline = Infinity;
      const sampleFormat = this.supportsIq10Packed ? "IQ10_PACKED" : null;
      const command = `${this.version2 ? "STREAM2" : "STREAM"} ${activeCenter} ${config.sampleCount}${sampleFormat ? ` ${sampleFormat}` : ""}\n`;
      this.log(command.trim());
      await this.transport.write(command);
      let previousSequence = null;
      while (true) {
        const deadline = Math.min(performance.now() + 8000, this.stopDeadline, tuneDeadline);
        const frame = await this.readFrame(deadline);
        if (frame.type === 2) {
          this.log("Stream stopped (BPRF end frame).");
          return;
        }
        if (frame.type === 3) throw adapterError(frame.flags);
        if (pendingCenter !== null && frame.centerHz === pendingCenter * 1e6) {
          activeCenter = pendingCenter;
          pendingCenter = null;
          tuneDeadline = Infinity;
          dwell = 0;
        }
        validateIqFrame(frame, { ...config, centerMHz: activeCenter }, previousSequence);
        const wirePayload = await this.transport.readExact(frame.payloadBytes, deadline);
        frame.wirePayloadBytes = frame.payloadBytes;
        frame.payload = decodeIqPayload(wirePayload, frame.sampleFormat ?? SAMPLE_FORMAT.RAW32, frame.count);
        previousSequence = frame.sequence;
        if (sweeping) Object.assign(frame, { sweepId, sliceIndex: index, captureIndex: dwell,
          sweepValid: !frame.retuned && pendingCenter === null && !this.stopRequested });
        if (frame.bandwidth) this.bandwidth = frame.bandwidth;
        onFrame(frame);
        if (!frame.retuned && pendingCenter === null) dwell++;
        if ((centers.length > 1 || sweeping) && dwell >= dwellFrames && pendingCenter === null && !this.stopRequested) {
          index = (index + 1) % centers.length;
          if (index === 0) sweepId++;
          if (centers.length === 1) { dwell = 0; continue; }
          pendingCenter = centers[index];
          tuneDeadline = performance.now() + 8000;
          await this.transport.write(`TUNE ${pendingCenter}\n`);
        }
      }
    } catch (error) {
      // Recover on a fresh connection after corrupted data, timeouts or unplugging.
      try { await this.stop(); } catch { /* Preserve the original stream error. */ }
      await this.transport.close();
      this.info = null;
      throw error;
    } finally {
      this.streaming = false;
    }
  }

  stop() {
    if (!this.streaming) return Promise.resolve();
    this.stopRequested = true;
    if (!this.stopTask) {
      this.log("STOP");
      this.stopDeadline = performance.now() + 8000;
      this.stopTask = this.transport.write("STOP\n");
    }
    return this.stopTask;
  }

  async disconnect() {
    this.disconnecting = true;
    try {
      if (this.streaming) {
        try { await this.stop(); } catch { await this.transport.close(); }
        try { await this.streamTask; } catch { /* Already reported by stream(). */ }
      }
    } finally {
      await this.transport.close();
      this.info = null;
    }
  }
}
