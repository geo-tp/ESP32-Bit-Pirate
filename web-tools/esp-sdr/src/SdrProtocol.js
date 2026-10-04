// SPDX-License-Identifier: MIT
export const DEFAULT_SAMPLE_RATE = 80_000_000;
export const SUPPORTED_SAMPLE_RATES = [80_000_000, 40_000_000, 16_000_000];
export const MAX_SAMPLES = 16_384;
// Only a fallback display crop for legacy firmware with no bandwidth metadata.
export const LEGACY_SPAN_MHZ = 24;
export const RECOMMENDED_BAND = [2400, 2483.5];
export const DEFAULT_TUNING_RANGE = Object.freeze([2200, 2800]);
export const EXTENDED_TUNING_RANGE = Object.freeze([100, 6000]);
export const HEADER_BYTES = 32;
export const BANDWIDTH_HEADER_BYTES = 40;
export const EXTENDED_HEADER_BYTES = 44;
export const IQ_FLAG_RETUNED = 1;
export const IQ_FORMAT_SHIFT = 8;
export const IQ_FORMAT_MASK = 0x0f00;
export const SAMPLE_FORMAT = Object.freeze({ RAW32: 0, IQ10_PACKED: 1 });
export const WATERFALL_ROWS = 320;
export const reliableHalfMHz = bandwidthHz => bandwidthHz / 1e6 * 0.4;
export const experimentalRange = (start, end) => start < RECOMMENDED_BAND[0] || end > RECOMMENDED_BAND[1];
export const bandwidthMHz = bandwidth => (bandwidth?.estimatedHz || bandwidth?.targetHz || LEGACY_SPAN_MHZ * 1e6) / 1e6;
export const formatMHz = value => Number(value.toFixed(2)).toString();

export function tuningLimits(info, extended = false, marginMHz = 0) {
  const requested = extended ? EXTENDED_TUNING_RANGE : DEFAULT_TUNING_RANGE;
  const adapterMin = Number(info?.center_min_mhz ?? EXTENDED_TUNING_RANGE[0]);
  const adapterMax = Number(info?.center_max_mhz ?? EXTENDED_TUNING_RANGE[1]);
  const min = Math.max(requested[0], adapterMin) + marginMHz;
  const max = Math.min(requested[1], adapterMax) - marginMHz;
  if (!(min < max)) {
    // Legacy/special adapters may not overlap the normal S3 UI range.
    return [adapterMin + marginMHz, adapterMax - marginMHz];
  }
  return [min, max];
}

export function bandwidthSetting(value, info) {
  const text = String(value).trim().toUpperCase();
  if (text === "AUTO" || text === "WIDE" || text === "0") return text === "0" ? "WIDE" : text;
  const number = Number(text);
  if (!/^\d+$/.test(text) || !Number.isInteger(number) || number < Number(info?.bw_min_mhz ?? 13) || number > Number(info?.bw_max_mhz ?? 69)) {
    throw new Error(`RX bandwidth: choose ${info?.bw_min_mhz ?? 13}–${info?.bw_max_mhz ?? 69} MHz, WIDE or AUTO.`);
  }
  return String(number);
}

export function parseBandwidth(line) {
  if (!line.startsWith("BANDWIDTH ")) throw new Error("Expected a BANDWIDTH response.");
  const fields = Object.fromEntries(line.slice(10).trim().split(/\s+/).map(item => item.split("=")));
  const result = { requestedMHz: Number(fields.requested_mhz), targetHz: Number(fields.target_hz),
    estimatedHz: Number(fields.estimated_hz), iCode: Number(fields.dcap_i), qCode: Number(fields.dcap_q),
    actualValid: fields.actual_valid === "1", approximate: true };
  if (![-1, 0].includes(result.requestedMHz) && !(result.requestedMHz >= 13 && result.requestedMHz <= 69 && Number.isInteger(result.requestedMHz))) throw new Error("Invalid RX bandwidth setting.");
  if (![result.targetHz, result.estimatedHz].every(n => Number.isInteger(n) && n >= 0 && n < 80_000_000)
      || ![result.iCode, result.qCode].every(n => Number.isInteger(n) && n >= 0 && n <= 255)) throw new Error("Invalid RX bandwidth metadata.");
  if (!result.actualValid) result.estimatedHz = 0;
  return result;
}
// Short FFTs are used by the coarse overview scan; Live exposes 1024 and up.
export const SAMPLE_COUNTS = [256, 512, 1024, 2048, 4096, 8192, 16384];

export function validateConfig(startMHz, endMHz, sampleCount, sampleRate = DEFAULT_SAMPLE_RATE) {
  if (!Number.isFinite(startMHz) || !Number.isFinite(endMHz) || startMHz <= 0 || endMHz <= startMHz) {
    throw new Error("Invalid frequency range: To must be greater than From.");
  }
  const spanMHz = endMHz - startMHz;
  if (!SUPPORTED_SAMPLE_RATES.includes(sampleRate)) throw new Error("Choose a supported sample rate.");
  if (spanMHz > sampleRate / 1e6) throw new Error(`Display width exceeds the ${sampleRate / 1e6} MS/s sampling span.`);
  if (!SAMPLE_COUNTS.includes(sampleCount)) throw new Error("Choose a supported sample count.");
  return { startMHz, endMHz, centerMHz: (startMHz + endMHz) / 2, spanMHz, sampleCount, sampleRate };
}

export function parseInfo(line) {
  if (!line.startsWith("INFO BPRF1 ")) throw new Error("Expected an INFO BPRF1 response.");
  const fields = Object.fromEntries(line.slice(11).trim().split(/\s+/).map(item => item.split("=")));
  if (fields.ready !== "1") throw new Error(`SDR radio is not ready: ${fields.init_error || line}`);
  const rate = Number(fields.fs_hz);
  if (!SUPPORTED_SAMPLE_RATES.includes(rate)) throw new Error("The adapter reports an unsupported sample rate.");
  if (!Number.isInteger(Number(fields.max_samples)) || Number(fields.max_samples) < 256) {
    throw new Error("Invalid adapter sample capacity.");
  }
  const min = Number(fields.center_min_mhz), max = Number(fields.center_max_mhz);
  if (!Number.isInteger(min) || !Number.isInteger(max) || min <= 0 || max * 1e6 > Number.MAX_SAFE_INTEGER || min >= max) {
    throw new Error("Invalid adapter tuning range.");
  }
  if (fields.bw_control === "1" && (Number(fields.bw_min_mhz) < 1 || Number(fields.bw_max_mhz) > 100
      || !Number.isInteger(Number(fields.bw_min_mhz)) || !Number.isInteger(Number(fields.bw_max_mhz))
      || Number(fields.bw_min_mhz) > Number(fields.bw_max_mhz))) throw new Error("Invalid RX bandwidth limits.");
  return fields;
}

export function parseRate(line) {
  if (!line.startsWith("RATE ")) throw new Error("Expected a RATE response.");
  const fields = Object.fromEntries(line.slice(5).trim().split(/\s+/).map(item => item.split("=")));
  const fsHz = Number(fields.fs_hz);
  if (!SUPPORTED_SAMPLE_RATES.includes(fsHz)) throw new Error("Invalid SDR sample rate response.");
  return { fsHz, supportedHz: String(fields.supported_hz || "").split(",").map(Number).filter(Number.isFinite) };
}

export function parseGain(line) {
  if (!line.startsWith("GAIN ")) throw new Error("Expected a GAIN response.");
  const fields = Object.fromEntries(line.slice(5).trim().split(/\s+/).map(item => item.split("=")));
  const supported = fields.supported === "1", max = Number(fields.max), index = Number(fields.index);
  if (!Number.isInteger(max) || max < 0 || max > 255) throw new Error("Invalid SDR gain response.");
  if (!["HARDWARE", "MANUAL"].includes(fields.mode)) throw new Error("Invalid SDR gain mode.");
  if (fields.mode === "MANUAL" && (!Number.isInteger(index) || index < 0 || index > max)) throw new Error("Invalid SDR gain index.");
  return { supported, mode: fields.mode, index, max };
}

export function headerBytesForVersion(version) {
  if (version === 1) return HEADER_BYTES;
  if (version === 2) return BANDWIDTH_HEADER_BYTES;
  if (version === 3) return EXTENDED_HEADER_BYTES;
  return 0;
}

export function iqPayloadBytes(format, sampleCount) {
  if (format === SAMPLE_FORMAT.RAW32) return sampleCount * 4;
  if (format === SAMPLE_FORMAT.IQ10_PACKED) return Math.ceil(sampleCount * 20 / 8);
  throw new Error(`Unsupported IQ sample format ${format}.`);
}

export function parseHeader(bytes) {
  if (![HEADER_BYTES, BANDWIDTH_HEADER_BYTES, EXTENDED_HEADER_BYTES].includes(bytes.byteLength)) throw new Error("Incomplete BPRF header.");
  if (bytes[0] !== 66 || bytes[1] !== 80 || bytes[2] !== 82 || bytes[3] !== 70 || ![1, 2, 3].includes(bytes[4])) {
    throw new Error("Invalid BPRF frame. Select SDR mode and reconnect to its CDC port.");
  }
  const expectedHeaderBytes = headerBytesForVersion(bytes[4]);
  if (bytes.byteLength !== expectedHeaderBytes) throw new Error("Invalid BPRF header length.");
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const centerLow = view.getUint32(12, true);
  const centerHigh = bytes[4] === 3 ? view.getUint32(40, true) : 0;
  const frame = {
    version: bytes[4], type: bytes[5], flags: view.getUint16(6, true), sequence: view.getUint32(8, true),
    centerHz: centerHigh * 0x100000000 + centerLow, sampleRate: view.getUint32(16, true),
    count: view.getUint32(20, true), captureUs: view.getUint32(24, true),
    payloadBytes: view.getUint32(28, true),
  };
  if (![1, 2, 3].includes(frame.type)) throw new Error(`Unknown BPRF frame type ${frame.type}.`);
  if (frame.payloadBytes > MAX_SAMPLES * 4 || frame.count > MAX_SAMPLES) {
    throw new Error("BPRF frame exceeds the maximum sample capacity.");
  }
  if (frame.type === 1) {
    const allowedFlags = IQ_FLAG_RETUNED | IQ_FORMAT_MASK;
    if (!frame.count || (frame.flags & ~allowedFlags)) throw new Error("Invalid IQ frame length or flags.");
    frame.sampleFormat = (frame.flags & IQ_FORMAT_MASK) >>> IQ_FORMAT_SHIFT;
    if (![SAMPLE_FORMAT.RAW32, SAMPLE_FORMAT.IQ10_PACKED].includes(frame.sampleFormat)
        || frame.payloadBytes !== iqPayloadBytes(frame.sampleFormat, frame.count)) {
      throw new Error("Invalid IQ frame length or sample format.");
    }
  }
  if (frame.type !== 1 && frame.payloadBytes !== 0) throw new Error("Control frame has an unexpected payload.");
  if (frame.type === 2 && (frame.count || frame.flags)) throw new Error("Invalid stream stop frame.");
  if (frame.version >= 2) {
    frame.bandwidth = { requestedMHz: view.getInt16(32, true), iCode: bytes[34], qCode: bytes[35],
      estimatedHz: view.getUint32(36, true), approximate: true };
    if (frame.type === 1 && (!(frame.bandwidth.estimatedHz > 0 && frame.bandwidth.estimatedHz < 80_000_000)
        || frame.bandwidth.iCode > 63 || frame.bandwidth.qCode > 63
        || !([-1, 0].includes(frame.bandwidth.requestedMHz) || frame.bandwidth.requestedMHz >= 13 && frame.bandwidth.requestedMHz <= 69))) {
      throw new Error("Invalid RX bandwidth readback in IQ frame.");
    }
  }
  frame.retuned = frame.type === 1 && Boolean(frame.flags & IQ_FLAG_RETUNED);
  return frame;
}

// Convert the compact firmware transport back to the native low-20-bit RF words
// expected by the existing DSP pipeline. The USB link stays packed; expansion is browser-only.
export function decodeIqPayload(payload, sampleFormat, sampleCount) {
  if (payload.byteLength !== iqPayloadBytes(sampleFormat, sampleCount)) throw new Error("IQ payload length does not match its sample format.");
  if (sampleFormat === SAMPLE_FORMAT.RAW32) return payload;
  if (sampleFormat !== SAMPLE_FORMAT.IQ10_PACKED) throw new Error(`Unsupported IQ sample format ${sampleFormat}.`);

  const output = new Uint8Array(sampleCount * 4);
  const words = new DataView(output.buffer);
  let source = 0, sample = 0;
  while (sample + 1 < sampleCount) {
    const a = payload[source] | (payload[source + 1] << 8) | ((payload[source + 2] & 0x0f) << 16);
    const b = (payload[source + 2] >>> 4) | (payload[source + 3] << 4) | (payload[source + 4] << 12);
    words.setUint32(sample * 4, a, true);
    words.setUint32((sample + 1) * 4, b, true);
    source += 5; sample += 2;
  }
  if (sample < sampleCount) {
    const a = payload[source] | (payload[source + 1] << 8) | ((payload[source + 2] & 0x0f) << 16);
    words.setUint32(sample * 4, a, true);
  }
  return output;
}

export function validateIqFrame(frame, config, previousSequence) {
  if (frame.sampleRate !== config.sampleRate || frame.count !== config.sampleCount) {
    throw new Error("Unexpected IQ sample rate or sample count.");
  }
  if (frame.centerHz !== config.centerMHz * 1_000_000) throw new Error("Unexpected center frequency.");
  if (previousSequence !== null && frame.sequence !== ((previousSequence + 1) >>> 0)) {
    throw new Error(`Capture sequence discontinuity: expected ${(previousSequence + 1) >>> 0}, received ${frame.sequence}.`);
  }
}

export function adapterError(code) {
  const reasons = ["Unknown error", "Radio not ready", "Invalid frequency", "Invalid sample count",
    "Capture engine busy", "Capture timeout", "Unchanged samples", "Invalid command",
    "Invalid command byte", "Command timeout", "RX filter readback failed"];
  return new Error(`SDR adapter: ${reasons[code] || "Unknown error"} (code ${code}).`);
}
