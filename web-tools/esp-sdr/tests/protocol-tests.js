import { SdrClient } from "../src/SdrClient.js";
import { SdrSerialTransport } from "../src/SdrSerialTransport.js";
import { Spectrum, estimateUsableBandwidth } from "../src/Spectrum.js";
import { SAMPLE_FORMAT, decodeIqPayload, parseHeader, parseInfo, parseBandwidth, parseRate, parseGain, bandwidthSetting, adapterError, tuningLimits, validateConfig, validateIqFrame } from "../src/SdrProtocol.js";

export const infoLine = "INFO BPRF1 ready=1 init_error=ESP_OK idf=v5.4 samples=256 max_samples=16380 fs_hz=80000000 rates_hz=80000000,40000000,16000000 center_min_mhz=2200 center_max_mhz=2800 gain_control=1 gain_max=82";
const encoder = new TextEncoder();

export function makeFrame({ type = 1, flags = 0, sequence = 19, centerHz = 2437000000,
  version = 1, requestedMHz = 40, estimatedHz = 40000000, iCode = 20, qCode = 20, sampleRate = 80000000, count = type === 1 ? 1024 : 0, captureUs = 52, payload = new Uint8Array(count * 4) } = {}) {
  const headerBytes = version === 3 ? 44 : version === 2 ? 40 : 32;
  const bytes = new Uint8Array(headerBytes + payload.length), view = new DataView(bytes.buffer);
  bytes.set([66, 80, 82, 70, version, type]);
  view.setUint16(6, flags, true);
  view.setUint32(8, sequence, true);
  view.setUint32(12, centerHz >>> 0, true);
  [sampleRate, count, captureUs, payload.length].forEach((n, i) => view.setUint32(16 + i * 4, n, true));
  if (version >= 2) {
    view.setInt16(32, requestedMHz, true); bytes[34] = iCode; bytes[35] = qCode;
    view.setUint32(36, estimatedHz, true);
  }
  if (version === 3) view.setUint32(40, Math.floor(centerHz / 0x100000000), true);
  bytes.set(payload, headerBytes);
  return bytes;
}

export function packedTone(count, bin = 71, amplitude = 200) {
  const data = new Uint8Array(count * 4), view = new DataView(data.buffer);
  for (let i = 0; i < count; i++) {
    const re = Math.round(amplitude * Math.cos(2 * Math.PI * bin * i / count));
    const im = Math.round(amplitude * Math.sin(2 * Math.PI * bin * i / count));
    view.setUint32(i * 4, ((re & 1023) | ((im & 1023) << 10)) >>> 0, true);
  }
  return data;
}

export function packIq10(raw) {
  const count = raw.byteLength / 4, input = new DataView(raw.buffer, raw.byteOffset, raw.byteLength);
  const packed = new Uint8Array(Math.ceil(count * 20 / 8));
  let source = 0, target = 0;
  while (source + 1 < count) {
    const a = input.getUint32(source * 4, true) & 0xfffff;
    const b = input.getUint32((source + 1) * 4, true) & 0xfffff;
    packed[target++] = a; packed[target++] = a >>> 8; packed[target++] = (a >>> 16) | (b << 4);
    packed[target++] = b >>> 4; packed[target++] = b >>> 12;
    source += 2;
  }
  if (source < count) {
    const a = input.getUint32(source * 4, true) & 0xfffff;
    packed[target++] = a; packed[target++] = a >>> 8; packed[target++] = a >>> 16;
  }
  return packed;
}

export class FakePort extends EventTarget {
  constructor({ fragment = 0, onWrite = null, info = infoLine } = {}) {
    super();
    this.fragment = fragment;
    this.onWrite = onWrite;
    this.info = info;
    this.commands = [];
    this.closed = false;
    this.readable = new ReadableStream({ start: controller => { this.controller = controller; } });
    this.writable = new WritableStream({ write: bytes => {
      const command = new TextDecoder().decode(bytes).trim();
      this.commands.push(command);
      if (command === "INFO") this.feed(encoder.encode(`READY BPRF1 ready=1\n${this.info}\nEND\n`));
      return this.onWrite?.(command, this);
    } });
  }
  async open(options) { this.options = options; }
  async setSignals(signals) { this.signals = signals; }
  async close() {
    assert(!this.readable.locked && !this.writable.locked, "Serial locks must be released before port.close()");
    this.closed = true;
  }
  feed(bytes) {
    if (this.fragment) {
      for (let i = 0; i < bytes.length; i += this.fragment) this.controller.enqueue(bytes.slice(i, i + this.fragment));
    } else this.controller.enqueue(bytes);
  }
}

function assert(value, message = "Assertion failed") { if (!value) throw new Error(message); }
function equal(a, b) { assert(JSON.stringify(a) === JSON.stringify(b), `${JSON.stringify(a)} != ${JSON.stringify(b)}`); }
function throws(fn, pattern) {
  try { fn(); } catch (error) { assert(pattern.test(error.message), error.message); return; }
  throw new Error("Expected an exception");
}
async function rejects(fn, pattern) {
  try { await fn(); } catch (error) { assert(pattern.test(error.message), error.message); return; }
  throw new Error("Expected a rejection");
}

export async function runTests(report = () => {}) {
  const tests = [];
  const test = (name, fn) => tests.push({ name, fn });
  test("Wideband tuning limits default to 2200–2800 and expose 100–6000 only in extended mode", () => {
    equal(tuningLimits(null, false), [2200, 2800]);
    equal(tuningLimits(null, true), [100, 6000]);
    equal(tuningLimits({ center_min_mhz: "100", center_max_mhz: "6000" }, false), [2200, 2800]);
    equal(tuningLimits({ center_min_mhz: "100", center_max_mhz: "6000" }, true), [100, 6000]);
    equal(tuningLimits({ center_min_mhz: "2200", center_max_mhz: "2800" }, true), [2200, 2800]);
  });
  test("Display spans are independent of integer tuning centers and support fractional bandwidth", () => {
    equal(validateConfig(2200, 2280, 1024).centerMHz, 2240);
    equal(validateConfig(2400, 2440, 4096).spanMHz, 40);
    equal(validateConfig(2798, 2800, 16384).spanMHz, 2);
    for (const [a, b] of [[2200, 2282], [2400, 2398], [NaN, 2440], [-1, 10]]) {
      throws(() => validateConfig(a, b, 1024), /range|width/);
    }
    equal(validateConfig(2165.5, 2234.5, 256).centerMHz, 2200);
    equal(validateConfig(2420.25, 2453.75, 512).spanMHz, 33.5);
    equal(validateConfig(2400, 2440, 256).sampleCount, 256);
    throws(() => validateConfig(2400, 2440, 128), /sample count/);
  });
  test("INFO readiness and sample rate validation", () => {
    equal(parseInfo(infoLine).ready, "1");
    throws(() => parseInfo(infoLine.replace("ready=1", "ready=0")), /not ready/);
    equal(parseInfo(infoLine.replace("fs_hz=80000000", "fs_hz=40000000")).fs_hz, "40000000");
    throws(() => parseInfo(infoLine.replace("fs_hz=80000000", "fs_hz=32000000")), /unsupported sample rate/);
    throws(() => parseInfo(infoLine.replace("max_samples=16380", "max_samples=no")), /capacity/);
    throws(() => parseInfo(infoLine.replace("center_min_mhz=2200", "center_min_mhz=oops")), /tuning/);
  });
  test("RATE and GAIN replies expose firmware controls", () => {
    equal(parseRate("RATE fs_hz=40000000 supported_hz=80000000,40000000,16000000 nominal=1").fsHz, 40000000);
    equal(parseGain("GAIN supported=1 mode=MANUAL index=40 max=82 calibrated_db=0").index, 40);
    equal(parseGain("GAIN supported=1 mode=HARDWARE index=-1 max=82 calibrated_db=0").mode, "HARDWARE");
    throws(() => parseRate("RATE fs_hz=32000000 supported_hz=32000000"), /Invalid SDR sample rate/);
    throws(() => parseGain("GAIN supported=1 mode=MANUAL index=90 max=82 calibrated_db=0"), /gain index/);
  });
  test("Little-endian headers preserve frequencies above signed 32-bit range", () => {
    const bytes = makeFrame({ sequence: 0xffffffff, centerHz: 2799000000 });
    const frame = parseHeader(bytes.subarray(0, 32));
    equal([frame.sequence, frame.centerHz, frame.captureUs, frame.payloadBytes], [4294967295, 2799000000, 52, 4096]);
  });
  test("Header rejects corruption before allocating payload", () => {
    for (const [offset, value] of [[0, 0], [4, 4], [5, 4]]) {
      const bytes = makeFrame().slice(0, 32); bytes[offset] = value;
      throws(() => parseHeader(bytes), /Invalid|Unknown/);
    }
    for (const [offset, value] of [[28, 0xffffffff], [20, 16385], [28, 4]]) {
      const bytes = makeFrame().slice(0, 32); new DataView(bytes.buffer).setUint32(offset, value, true);
      throws(() => parseHeader(bytes), /capacity|length/);
    }
    throws(() => parseHeader(makeFrame({ type: 2, payload: new Uint8Array(4) }).slice(0, 32)), /payload/);
    assert(parseHeader(makeFrame({ flags: 1 }).slice(0, 32)).retuned, "Firmware retune bit must be accepted");
    throws(() => parseHeader(makeFrame({ flags: 2 }).slice(0, 32)), /flags/);
  });
  test("BPRF v2 exposes signed AUTO, actual filter readback, and accepts empty control metadata", () => {
    const frame = parseHeader(makeFrame({version:2, requestedMHz:-1, estimatedHz:68500000}).subarray(0,40));
    equal([frame.version, frame.bandwidth.requestedMHz, frame.bandwidth.estimatedHz], [2,-1,68500000]);
    throws(() => parseHeader(makeFrame({version:2}).subarray(0,32)), /length/);
    throws(() => parseHeader(makeFrame({version:2,estimatedHz:80000000}).subarray(0,40)), /bandwidth/);
    throws(() => parseHeader(makeFrame({version:2,iCode:255}).subarray(0,40)), /bandwidth/);
    equal(parseHeader(makeFrame({version:2,type:2,estimatedHz:0,iCode:255,qCode:255}).subarray(0,40)).type,2);
    assert(adapterError(10).message.includes("RX filter readback"));
  });
  test("Extended INFO and BPRF v3 preserve 100–6000 MHz tuning with 64-bit center Hz", () => {
    const extended = parseInfo(infoLine.replace("center_min_mhz=2200 center_max_mhz=2800", "center_min_mhz=100 center_max_mhz=6000"));
    equal([extended.center_min_mhz, extended.center_max_mhz], ["100", "6000"]);
    const frame = parseHeader(makeFrame({ version: 3, centerHz: 6_000_000_000 }).subarray(0, 44));
    equal([frame.version, frame.centerHz, frame.bandwidth.estimatedHz], [3, 6_000_000_000, 40_000_000]);
  });
  test("IQ10 packed decoding preserves every 10-bit I/Q word, including an odd final sample", () => {
    for (const count of [1, 2, 3, 17, 256]) {
      const raw = packedTone(count, Math.max(1, count >> 2), 255);
      const packed = packIq10(raw);
      const decoded = decodeIqPayload(packed, SAMPLE_FORMAT.IQ10_PACKED, count);
      equal([...decoded], [...raw]);
      const header = makeFrame({ count, flags: SAMPLE_FORMAT.IQ10_PACKED << 8, payload: packed }).subarray(0, 32);
      equal(parseHeader(header).payloadBytes, packed.length);
    }
  });
  test("New firmware automatically uses fragmented IQ10_PACKED transport and BPRF v3 above 4.294 GHz", async () => {
    const count = 1024, center = 5000, raw = packedTone(count), packed = packIq10(raw);
    const info = infoLine.replace("center_min_mhz=2200 center_max_mhz=2800", "center_min_mhz=100 center_max_mhz=6000")
      + " frame_versions=1,2,3 sample_formats=RAW32,IQ10_PACKED default_sample_format=RAW32";
    const port = new FakePort({ fragment: 7, info, onWrite(command, device) {
      if (command.startsWith("STREAM2")) {
        assert(command === `STREAM2 ${center} ${count} IQ10_PACKED`);
        device.feed(makeFrame({ version: 3, centerHz: center * 1e6, count, flags: SAMPLE_FORMAT.IQ10_PACKED << 8, payload: packed }));
      }
      if (command === "STOP") device.feed(makeFrame({ version: 3, type: 2, centerHz: center * 1e6, estimatedHz: 0 }));
    }});
    const client = new SdrClient(); await client.connect(port);
    await client.stream(validateConfig(center - 34, center + 34, count), frame => {
      equal(frame.wirePayloadBytes, packed.length); equal([...frame.payload], [...raw]); void client.stop();
    });
    await client.disconnect();
  });
  test("Bandwidth commands validate limits and distinguish stale readback from a new target", async () => {
    const info = infoLine + " bw_control=1 bw_min_mhz=13 bw_max_mhz=69 bw_requested_mhz=-1 frame_versions=1,2";
    const port = new FakePort({info, onWrite(command, device) {
      if (command.startsWith("BANDWIDTH")) device.feed(encoder.encode(`BANDWIDTH requested_mhz=${command === "BANDWIDTH?" ? -1 : 0} target_hz=${command === "BANDWIDTH?" ? 0 : 69000000} estimated_hz=24000000 dcap_i=31 dcap_q=31 actual_valid=1 approximate=1\nEND\n`));
    }});
    const client = new SdrClient(); await client.connect(port);
    equal(client.bandwidth.estimatedHz,24000000);
    await client.configureBandwidth("WIDE");
    equal([client.bandwidth.estimatedHz,client.bandwidth.targetHz],[0,69000000]);
    for (const value of [12,70,13.5,"13; REBOOT"]) throws(() => bandwidthSetting(value,client.info), /bandwidth/);
    equal(bandwidthSetting("0",client.info),"WIDE"); equal(bandwidthSetting("auto",client.info),"AUTO");
    throws(() => parseBandwidth("BANDWIDTH requested_mhz=40 target_hz=NaN estimated_hz=0"), /metadata/);
    await client.disconnect();
  });
  for (const fragment of [1,7,39,4096]) test(`STREAM2 consumes fragmented ${fragment}-byte IQ and END headers across restart`, async () => {
    let sequence=0;
    const port = new FakePort({fragment, info:infoLine+" frame_versions=1,2", onWrite(command,device) {
      if (command.startsWith("STREAM2")) device.feed(makeFrame({version:2,sequence:sequence++,count:Number(command.split(" ")[2]),estimatedHz:68000000}));
      if (command === "STOP") device.feed(makeFrame({version:2,type:2,estimatedHz:0}));
    }});
    const client=new SdrClient(); await client.connect(port);
    for(let i=0;i<2;i++) await client.stream(validateConfig(2403,2471,1024),frame=>{
      equal(frame.payload.length,4096); equal(frame.bandwidth.estimatedHz,68000000); void client.stop();
    });
    equal(port.commands.filter(c=>c==="STOP").length,2); equal(client.bandwidth.estimatedHz,68000000);
    await client.disconnect();
  });
  test("Bandwidth probe drains v2 END and disconnect never starts another acquisition", async () => {
    const port=new FakePort({info:infoLine+" frame_versions=1,2",onWrite(command,device){
      if(command.startsWith("STREAM2")) device.feed(makeFrame({version:2,count:256,estimatedHz:68500000}));
      if(command==="STOP") device.feed(makeFrame({version:2,type:2,estimatedHz:0}));
    }});
    const client=new SdrClient(); await client.connect(port);
    await client.measureBandwidth(2437);
    assert(!client.streaming); equal(client.bandwidth.estimatedHz,68500000);
    await client.disconnect();
    throws(()=>client.startStream({sampleCount:256},[2437],()=>{}),/Connect|running/);
    equal(port.commands.filter(c=>c.startsWith("STREAM2")).length,1);
  });
  test("RX_FILTER_READBACK from a v2 error frame closes the connection", async () => {
    const port=new FakePort({info:infoLine+" frame_versions=1,2",onWrite(command,device){
      if(command.startsWith("STREAM2")) device.feed(makeFrame({version:2,type:3,flags:10,estimatedHz:0}));
    }});
    const client=new SdrClient(); await client.connect(port);
    await rejects(()=>client.stream(validateConfig(2403,2471,1024),()=>{}),/RX filter readback/);
    assert(port.closed && !client.info);
  });
  test("STREAM/TUNE follows actual centers, drains old captures, and stops once", async () => {
    let sequence = 0, center = 2435;
    const port = new FakePort({ fragment: 63, onWrite: (command, device) => {
      const feed = (flags = 0) => device.feed(makeFrame({ flags, centerHz: center * 1e6, sequence: sequence++ }));
      if (command.startsWith("STREAM")) { feed(1); for (let i = 0; i < 4; i++) feed(); }
      if (command.startsWith("TUNE")) {
        feed(); feed(); // In-flight USB frames still carry the previous LO.
        center = Number(command.split(" ")[1]);
        feed(1); for (let i = 0; i < 4; i++) feed();
      }
      if (command === "STOP") device.feed(makeFrame({ type: 2, centerHz: center * 1e6 }));
    } });
    const client = new SdrClient(); await client.connect(port);
    const seen = [], flags = [];
    await client.stream(validateConfig(2425, 2449, 1024), frame => {
      seen.push(frame.centerHz / 1e6); flags.push(frame.retuned);
      if (frame.centerHz === 2439000000 && !frame.retuned) { void client.stop(); void client.stop(); }
    }, [-2, 0, 2]);
    equal([...new Set(seen)], [2435, 2437, 2439]);
    equal(flags.filter(Boolean).length, 3);
    equal(port.commands, ["INFO", "STREAM 2435 1024", "TUNE 2437", "TUNE 2439", "STOP"]);
    await client.disconnect();
  });
  test("Unexpected tuning center closes the stream instead of shifting RF silently", async () => {
    const port = new FakePort({ onWrite: (command, device) => {
      if (command.startsWith("STREAM")) device.feed(makeFrame({ centerHz: 2441000000 }));
    } });
    const client = new SdrClient(); await client.connect(port);
    await rejects(() => client.stream(validateConfig(2425, 2449, 1024), () => {}, [-2, 0, 2]), /frequency/);
    assert(port.closed);
  });
  test("One transport can reopen and release a second port", async () => {
    const transport = new SdrSerialTransport();
    const first = new FakePort(), second = new FakePort();
    await transport.open(first); await transport.close();
    await transport.open(second); await transport.close();
    assert(first.closed && second.closed);
  });
  test("IQ validation and sequence wraparound", () => {
    const config = validateConfig(2397, 2477, 1024);
    const frame = parseHeader(makeFrame({ sequence: 0 }).slice(0, 32));
    validateIqFrame(frame, config, 0xffffffff);
    throws(() => validateIqFrame(frame, config, 1), /sequence/);
    throws(() => validateIqFrame({ ...frame, centerHz: 2412000000 }, config, null), /frequency/);
    throws(() => validateIqFrame({ ...frame, count: 2048 }, config, null), /count/);
    throws(() => validateIqFrame({ ...frame, sampleRate: 1 }, config, null), /rate/);
  });
  for (const fragment of [0, 1, 7, 64, 4096]) {
    test(`CDC stream handles ${fragment || "coalesced"}-byte chunks and consumes END`, async () => {
      const iq = makeFrame(), stop = makeFrame({ type: 2 });
      const port = new FakePort({ fragment, onWrite: (command, device) => {
        if (command.startsWith("STREAM")) {
          const both = new Uint8Array(iq.length + stop.length); both.set(iq); both.set(stop, iq.length); device.feed(both);
        }
      } });
      const client = new SdrClient();
      await client.connect(port);
      let frames = 0;
      await client.stream(validateConfig(2397, 2477, 1024), frame => { frames++; equal(frame.payload.length, 4096); });
      equal(frames, 1); assert(!client.streaming);
      equal(port.signals, { dataTerminalReady: true, requestToSend: false });
      equal(port.options.baudRate, 115200);
      await client.disconnect(); assert(port.closed);
    });
  }
  test("Stop is sent once; a stopped connection can start again", async () => {
    const port = new FakePort({ onWrite: (command, device) => {
      if (command.startsWith("STREAM")) device.feed(makeFrame());
      if (command === "STOP") device.feed(makeFrame({ type: 2 }));
    } });
    const client = new SdrClient(); await client.connect(port);
    for (let i = 0; i < 2; i++) {
      await client.stream(validateConfig(2397, 2477, 1024), () => { void client.stop(); void client.stop(); });
    }
    equal(port.commands, ["INFO", "STREAM 2437 1024", "STOP", "STREAM 2437 1024", "STOP"]);
    await client.disconnect();
  });
  test("Disconnect during capture sends STOP and waits for the end frame", async () => {
    const port = new FakePort({ onWrite: (command, device) => {
      if (command === "STOP") device.feed(makeFrame({ type: 2 }));
    } });
    const client = new SdrClient(); await client.connect(port);
    const streaming = client.stream(validateConfig(2397, 2477, 1024), () => {});
    await client.disconnect(); await streaming;
    equal(port.commands.slice(-2), ["STREAM 2437 1024", "STOP"]); assert(port.closed);
  });
  test("Capture errors close the port and report the firmware error", async () => {
    const port = new FakePort({ onWrite: (command, device) => {
      if (command.startsWith("STREAM")) device.feed(makeFrame({ type: 3, flags: 5, count: 1024, payload: new Uint8Array() }));
    } });
    const client = new SdrClient(); await client.connect(port);
    await rejects(() => client.stream(validateConfig(2397, 2477, 1024), () => {}), /Capture timeout/);
    assert(port.closed && !client.info && !client.streaming);
  });
  test("Sequence gaps fail without passing the corrupt frame to FFT", async () => {
    const port = new FakePort({ onWrite: (command, device) => {
      if (command.startsWith("STREAM")) { device.feed(makeFrame({ sequence: 1 })); device.feed(makeFrame({ sequence: 3 })); }
    } });
    const client = new SdrClient(); await client.connect(port); let frames = 0;
    await rejects(() => client.stream(validateConfig(2397, 2477, 1024), () => frames++), /discontinuity/);
    equal(frames, 1); assert(port.closed);
  });
  test("Unplug mid-payload rejects and releases all serial locks", async () => {
    const port = new FakePort({ onWrite: (command, device) => {
      if (command.startsWith("STREAM")) { device.feed(makeFrame().slice(0, 33)); device.controller.close(); }
    } });
    const client = new SdrClient(); await client.connect(port);
    await rejects(() => client.stream(validateConfig(2397, 2477, 1024), () => {}), /disconnected/);
    assert(port.closed);
  });
  test("Handshake failure closes the port", async () => {
    const port = new FakePort({ info: infoLine.replace("ready=1", "ready=0") });
    await rejects(() => new SdrClient().connect(port), /not ready/); assert(port.closed);
  });
  test("An incomplete INFO terminator never starts binary mode", async () => {
    const transport = { open: async () => {}, write: async () => {}, close: async () => {},
      lines: [infoLine], readLine: async function () { if (this.lines.length) return this.lines.shift(); throw new Error("No END"); } };
    await rejects(() => new SdrClient({ transport }).connect({}), /No END/);
  });
  test("Timeout during a partial frame is bounded and close cancels the pending read", async () => {
    const port = new FakePort(); const transport = new SdrSerialTransport(); await transport.open(port);
    port.feed(new Uint8Array([1, 2]));
    await rejects(() => transport.readExact(32, performance.now() + 20), /Timed out/);
    await transport.close(); assert(port.closed);
  });
  test("FFT zero input has the same -200 dBFS floor as Python", () => {
    const fft = new Spectrum(validateConfig(2397, 2477, 1024));
    assert(fft.compute(new Uint8Array(4096)).every(value => value === -200));
  });
  test("Signed 10-bit extremes, little endian and full-scale normalization", () => {
    const fft = new Spectrum(validateConfig(2397, 2477, 1024));
    for (const [re, im] of [[-512, 0], [511, -512], [-1, 1]]) {
      const payload = new Uint8Array(4096), view = new DataView(payload.buffer);
      for (let i = 0; i < 1024; i++) view.setUint32(i * 4, (re & 1023) | ((im & 1023) << 10) | 0xa0000000, true);
      const expected = 20 * Math.log10(Math.hypot(re, im) / 512);
      assert(Math.abs(fft.compute(payload)[512] - expected) < 0.0001, "Signed IQ or normalization mismatch");
    }
  });
  for (const n of [1024, 2048, 4096, 8192, 16384]) {
    test(`FFT ${n}: positive and negative tones, Hann normalization and crop`, () => {
      for (const bin of [-71, 71]) {
        const fft = new Spectrum(validateConfig(2397, 2477, n));
        const power = fft.compute(packedTone(n, bin));
        const peak = power.indexOf(Math.max(...power));
        // packedTone is I+jQ with positive sample rotation; native S3 RF is opposite.
        equal(peak, n / 2 - bin);
        assert(Math.abs(power[peak] - 20 * Math.log10(200 / 512)) < .01);
        equal(fft.firstMHz, 2397);
      }
      const cropped = new Spectrum(validateConfig(2400, 2402, n));
      const power = cropped.compute(packedTone(n, 0));
      assert(cropped.firstMHz >= 2400 && cropped.firstMHz + (power.length - 1) * cropped.binMHz < 2402);
      equal(power.length, Math.ceil(n / 2 + n / 80) - Math.ceil(n / 2 - n / 80));
    });
  }
  test("DC removal cancels a constant IQ offset and the baseline estimate stays centered", () => {
    const n = 1024, fft = new Spectrum(validateConfig(2397, 2477, n));
    const payload = new Uint8Array(n * 4), view = new DataView(payload.buffer);
    for (let i = 0; i < n; i++) view.setUint32(i * 4, (100 & 1023) | ((50 & 1023) << 10), true);
    assert(fft.compute(payload, true).every(value => value === -200));
    assert(fft.compute(payload, false)[n / 2] > -30);
    const offsets = Array.from({ length: n }, (_, i) => (i - n / 2) * 80 / n);
    const [lower, upper] = estimateUsableBandwidth(offsets, new Float32Array(n).fill(-60), 6);
    equal(lower, offsets[0]); equal(upper, offsets[n - 1]);
  });
  test("FFT agrees with an independent direct DFT on arbitrary signed IQ", () => {
    const n = 1024, fft = new Spectrum(validateConfig(2397, 2477, n));
    const payload = new Uint8Array(n * 4), view = new DataView(payload.buffer);
    for (let i = 0; i < n; i++) view.setUint32(i * 4, ((i * 37) % 1024) | (((i * 91) % 1024) << 10), true);
    const actual = fft.compute(payload);
    for (const k of [0, 1, 37, 71, 256, 511, 512, 777, 1023]) {
      let real = 0, imag = 0, sum = 0;
      for (let j = 0; j < n; j++) {
        const word = view.getUint32(j * 4, true), window = Math.fround(.5 - .5 * Math.cos(2 * Math.PI * j / (n - 1)));
        const a = Math.fround((((word & 1023) ^ 512) - 512) * window);
        const b = Math.fround(-((((word >>> 10) & 1023) ^ 512) - 512) * window);
        const phase = -2 * Math.PI * k * j / n;
        real += a * Math.cos(phase) - b * Math.sin(phase);
        imag += a * Math.sin(phase) + b * Math.cos(phase); sum += window;
      }
      const expected = 20 * Math.log10(Math.max(Math.hypot(real, imag) / (sum * 512), 1e-10));
      assert(Math.abs(actual[(k + n / 2) % n] - expected) < .0001, `DFT mismatch at ${k}`);
    }
  });
  let failures = 0;
  for (const { name, fn } of tests) {
    try { await fn(); report(`PASS ${name}`); }
    catch (error) { failures++; report(`FAIL ${name}: ${error.stack || error.message}`); }
  }
  return { total: tests.length, failures };
}
