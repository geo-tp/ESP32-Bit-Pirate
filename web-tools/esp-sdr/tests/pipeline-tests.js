import { Pipeline } from "../src/Pipeline.js";
import { Spectrum } from "../src/Spectrum.js";
import { validateConfig } from "../src/SdrProtocol.js";

function assert(value, message = "Assertion failed") { if (!value) throw new Error(message); }
function near(actual, expected, tolerance = 0.01) { assert(Math.abs(actual - expected) < tolerance, `${actual} != ${expected} (±${tolerance})`); }
const config = (n = 4096) => validateConfig(2425, 2449, n);

export function signal(n, { tones = [], noise = 0, seed = 1, dcI = 0, dcQ = 0 } = {}) {
  const payload = new Uint8Array(4 * n), view = new DataView(payload.buffer);
  const random = () => { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed / 2 ** 32 - 0.5; };
  for (let i = 0; i < n; i++) {
    let re = dcI + random() * noise, im = dcQ + random() * noise;
    for (const { bin, amplitude, phase = 0 } of tones) {
      const angle = 2 * Math.PI * bin * i / n + phase;
      // Native S3 Q rotates oppositely to RF offset (ESPARGOS capture convention).
      re += amplitude * Math.cos(angle); im -= amplitude * Math.sin(angle);
    }
    re = Math.max(-512, Math.min(511, Math.round(re))); im = Math.max(-512, Math.min(511, Math.round(im)));
    view.setUint32(i * 4, (re & 1023) | ((im & 1023) << 10), true);
  }
  return payload;
}
function peakNear(frame, mhz, radius = 3) {
  const index = Math.round((mhz - frame.firstMHz) / frame.binMHz);
  return Math.max(...frame.corrected.slice(index - radius, index + radius + 1));
}

export async function runPipelineTests(report = () => {}) {
  const tests = [], test = (name, fn) => tests.push({ name, fn });
  test("Native S3 IQ puts channels 1 and 10 on the correct RF side at multiple LOs, including Welch", () => {
    for (const n of [256,4096]) for (const center of [2427,2437,2442]) for (const rf of [2412,2457]) {
      // Independent wire fixture: a higher RF frequency rotates native Q negatively.
      const payload=new Uint8Array(n*4), words=new DataView(payload.buffer);
      for(let i=0;i<n;i++) {
        const angle=2*Math.PI*(rf-center)/80*i;
        words.setUint32(i*4,(Math.round(180*Math.cos(angle))&1023)|((Math.round(-180*Math.sin(angle))&1023)<<10),true);
      }
      for(const mode of ["instant","welch"]) {
        const pipe=new Pipeline(validateConfig(center-34.5,center+34.5,n),{mode});
        const frame=pipe.process(payload,center);
        for(const key of ["raw","dc","corrected"]) {
          const peak=frame[key].indexOf(Math.max(...frame[key]));
          near(frame.firstMHz+peak*frame.binMHz,rf,80/n+0.001);
        }
      }
    }
  });
  test("Persistent RF carriers survive default processing without a noise reference", () => {
    const pipe = new Pipeline(config(), { mode: "instant" });
    const payload = signal(4096, { tones: [{ bin: 256, amplitude: 150 }], noise: 4 });
    const first = pipe.process(payload, 2437);
    let last;
    for (let i = 0; i < 160; i++) last = pipe.process(payload, 2437);
    near(peakNear(last, 2442), peakNear(first, 2442));
    assert(!last.status.spur.active, "A fixed live carrier is not evidence of a receiver spur");
  });
  test("One-click cleaning learns and freezes a multi-capture live reference; new activity stays visible", () => {
    for (const mode of ["instant", "welch"]) for (const window of ["hann", "blackman-harris"]) {
      const pipe = new Pipeline(config(), { mode, window, cleanup: true, cleanupFrames: 8 });
      const tone = { bin: 256.4, amplitude: 120 };
      assert(pipe.process(signal(4096),2437,true) === null);
      assert(pipe.cleanupStatus().count === 0);
      for (let i=0;i<7;i++) pipe.process(signal(4096,{noise:30,seed:i+1,tones:[{...tone,phase:i}]}),2437);
      assert(pipe.cleanupStatus().state === "collecting" && pipe.cleanupStatus().count === 7);
      const learned = pipe.process(signal(4096,{noise:30,seed:8,tones:[tone]}),2437);
      assert(learned.status.cleanup.state === "ready" && learned.status.baseline === "none");
      assert(learned.correctedUnit === "dB above reference" && learned.status.cleanup.flagged > 0);
      assert(learned.status.lower === -40 && learned.status.upper === 40, "Live input cannot establish measured RF edges");
      const reference = pipe.cleanupReference;
      const data = signal(4096,{noise:30,seed:99,tones:[tone,{bin:-256,amplitude:120}]});
      const cleaned = pipe.process(data,2437);
      assert(peakNear(cleaned,2432) > peakNear(cleaned,2442)+15, "A new carrier must survive; the learned artifact must not");
      const raw = cleaned.raw.slice(), dc = cleaned.dc.slice();
      for (let i=0;i<20;i++) pipe.process(data,2437);
      assert(pipe.cleanupReference === reference, "Reference must stop learning after completion");
      pipe.setOptions({cleanup:false});
      const restored = pipe.process(data,2437);
      assert(restored.raw.every((v,i)=>Object.is(v,raw[i])) && restored.dc.every((v,i)=>Object.is(v,dc[i])));
      assert(!pipe.cleanupReference && !pipe.cleanupCalibration && restored.status.baseline === "none");
    }
  });
  test("Quick reference reuses calibration's power averaging without replacing a noise-only reference", () => {
    const pipe = new Pipeline(config(), { mode:"instant", cleanupFrames:8 });
    const other = new Pipeline(config(), { mode:"instant" });
    pipe.startCalibration(8);
    for(let i=0;i<8;i++) pipe.process(signal(4096,{noise:40,seed:i+1}),2437);
    const baseline=pipe.baseline;
    pipe.setOptions({cleanup:true}); other.startCalibration(8);
    for(let i=0;i<8;i++) {
      const payload=signal(4096,{noise:30,seed:i+30,tones:[{bin:256,amplitude:100,phase:i}]});
      pipe.process(payload,2437); other.process(payload,2437);
    }
    for(const key of ["power","welch","mask","welchMask"]) {
      assert(pipe.cleanupReference.get(2437)[key].every((v,i)=>v===other.baseline.get(2437)[key][i]));
    }
    assert(pipe.baseline===baseline);
    pipe.setOptions({cleanup:false}); assert(pipe.baseline===baseline);
    pipe.setOptions({cleanup:true});
    pipe.process(signal(4096,{noise:30}),2437);
    pipe.setOptions({cleanup:false});
    assert(!pipe.cleanupCalibration && !pipe.cleanupReference, "Cancellation discards partial measurements");
  });
  test("RX changes invalidate learned cleanup and held peaks, then learn a fresh reference", () => {
    const pipe = new Pipeline(config(), { mode:"peak", cleanup:true, cleanupFrames:8 });
    const payload=signal(4096,{noise:30,tones:[{bin:256,amplitude:100}]});
    const bw={estimatedHz:40000000,requestedMHz:40,iCode:20,qCode:20};
    for(let i=0;i<8;i++) pipe.process(payload,2437,false,bw);
    assert(pipe.cleanupReference);
    pipe.process(payload,2437,true,{...bw,estimatedHz:39000000});
    assert(!pipe.cleanupReference && pipe.cleanupStatus().count===0 && pipe.slots.size===0);
    pipe.setConfig(validateConfig(2430,2454,4096));
    assert(pipe.cleanupStatus().count===0 && pipe.centers[0]===2442);
  });
  test("Averaging is in linear power, peak hold retains bursts and reset clears them", () => {
    const pipe = new Pipeline(config(), { mode: "average", alpha: 0.5, spur: false });
    const payload = amplitude => signal(4096, { tones: [{ bin: 256, amplitude }] });
    const a = pipe.process(payload(100), 2437);
    const b = new Pipeline(config(), { mode: "instant" }).process(payload(200), 2437);
    const average = pipe.process(payload(200), 2437);
    const j = a.corrected.indexOf(Math.max(...a.corrected));
    near(10 ** (average.corrected[j] / 10), (10 ** (a.corrected[j] / 10) + 10 ** (b.corrected[j] / 10)) / 2, 1e-7);
    pipe.setOptions({ mode: "peak" });
    const peak = pipe.process(payload(200), 2437);
    near(peakNear(pipe.process(payload(10), 2437), 2442), peakNear(peak, 2442));
    pipe.reset();
    assert(peakNear(pipe.process(payload(10), 2437), 2442) < peakNear(peak, 2442) - 20);
  });
  test("Noise reference uses power, handles arbitrary capture phase, and resets units", () => {
    const pipe = new Pipeline(config(), { mode: "average", spur: false });
    pipe.process(signal(4096, { noise: 100 }), 2437);
    pipe.startCalibration(8);
    const payload = signal(4096, { noise: 16, tones: [{ bin: 32, amplitude: 100 }] });
    let frame;
    for (let i = 0; i < 8; i++) frame = pipe.process(payload, 2437);
    assert(frame.status.baseline === "ready" && frame.correctedUnit === "dB above reference");
    near(peakNear(frame, 2437.625), 0, 0.001); // Coherent subtraction incorrectly erases this reference.
    const inverted = payload.slice(), view = new DataView(inverted.buffer);
    for (let i = 0; i < 4096; i++) {
      const word = view.getUint32(i * 4, true);
      view.setUint32(i * 4, ((-((word & 1023) ^ 512) + 512) & 1023)
        | (((-(((word >>> 10) & 1023) ^ 512) + 512) & 1023) << 10), true);
    }
    near(peakNear(pipe.process(inverted, 2437), 2437.625), 0, 0.001);
    pipe.clearBaseline();
    frame = pipe.process(payload, 2437);
    assert(frame.correctedUnit === "dBFS" && frame.status.baseline === "none");
    assert(peakNear(frame, 2437.625) < -10);
  });
  test("Calibration is per center; retune captures never contaminate it", () => {
    const pipe = new Pipeline(config(), { diversity: true, mode: "instant", spur: false });
    pipe.startCalibration(8);
    // A centered impulse gives a deterministic broadband reference with distinct power per LO.
    const payload = center => {
      const data = new Uint8Array(4096 * 4);
      new DataView(data.buffer).setUint32(2048 * 4, (center - 2433) * 40, true);
      return data;
    };
    assert(pipe.process(payload(2435), 2435, true) === null);
    assert(pipe.status().count === 0);
    for (let i = 0; i < 8; i++) pipe.process(payload(2435), 2435);
    assert(pipe.status().baseline === "collecting" && pipe.status().count === 8 && pipe.status().target === 24);
    for (const center of [2437, 2439]) for (let i = 0; i < 8; i++) pipe.process(payload(center), center);
    assert(pipe.status().baseline === "ready");
    pipe.reset();
    let frame;
    for (const center of [2435, 2437, 2439]) frame = pipe.process(payload(center), center);
    assert(frame.analysisFresh);
    assert(frame.corrected.filter(Number.isFinite).length > 100);
    for (const value of frame.corrected) if (Number.isFinite(value)) near(value, 0, 0.001);
  });
  test("Reference spur suppression removes a calibrated birdie while preserving new RF", () => {
    const pipe = new Pipeline(config(), { mode: "instant", spur: true });
    pipe.startCalibration(64);
    for (let i = 0; i < 64; i++) pipe.process(signal(4096, {
      noise: 40, seed: i + 1, tones: [{ bin: 256, amplitude: 80, phase: i }]
    }), 2437);
    const payload = signal(4096, { noise: 40, seed: 1000, tones: [
      { bin: 256, amplitude: 200 }, { bin: -256, amplitude: 160 }
    ] });
    const suppressed = pipe.process(payload, 2437);
    assert(suppressed.status.spur.flagged > 0);
    pipe.setOptions({ spur: false });
    const unsuppressed = pipe.process(payload, 2437);
    assert(peakNear(unsuppressed, 2442) > peakNear(suppressed, 2442) + 3);
    near(peakNear(suppressed, 2432), peakNear(unsuppressed, 2432));
    assert(peakNear(suppressed, 2432) > 30);
  });
  test("Tune diversity keeps fixed RF, rejects moving LO birdies, and waits for fresh cycles", () => {
    const pipe = new Pipeline(config(), { diversity: true, mode: "instant" });
    let frame;
    for (const center of [2435, 2437, 2439]) {
      const payload = signal(4096, { tones: [
        { bin: (2442 - center) / 80 * 4096, amplitude: 160 },
        { bin: -5 / 80 * 4096, amplitude: 100 }
      ], noise: 8, seed: center });
      frame = pipe.process(payload, center);
      assert(frame.analysisFresh === (center === 2439));
    }
    assert(peakNear(frame, 2442) > -16, "RF carrier must stay at 2442 MHz");
    for (const birdie of [2430, 2432, 2434]) assert(peakNear(frame, birdie) < -40, `LO spur leaked at ${birdie}`);
    frame = pipe.process(signal(4096, { noise: 8 }), 2435);
    assert(!frame.analysisFresh, "Do not paint a new composite using stale tuning slots");
  });
  test("Calibration survives display crop, but center, size and window changes invalidate it", () => {
    const pipe = new Pipeline(config(), { spur: false });
    pipe.startCalibration(8);
    for (let i = 0; i < 8; i++) pipe.process(signal(4096, { noise: 24, seed: i + 1 }), 2437);
    const baseline = pipe.baseline;
    pipe.setConfig(validateConfig(2432, 2442, 4096));
    assert(pipe.baseline === baseline);
    pipe.setOptions({ window: "blackman-harris" });
    assert(pipe.baseline === null);
    pipe.setConfig(config(16384));
    assert(pipe.process(signal(16384), 2437).raw.length === Math.ceil(24 / (80 / 16384)));
    pipe.setConfig(validateConfig(2430, 2454, 16384));
    assert(pipe.baseline === null && pipe.calibration === null);
  });
  test("RX readback changes invalidate calibration even when the displayed width stays constant", () => {
    const pipe = new Pipeline(validateConfig(2403,2471,1024), {mode:"peak",spur:false});
    const reading={requestedMHz:0,estimatedHz:68000000,iCode:0,qCode:0};
    const payload=signal(1024,{noise:30});
    pipe.process(payload,2437,false,reading); pipe.startCalibration(8);
    for(let i=0;i<8;i++) pipe.process(payload,2437,false,reading);
    assert(pipe.baseline);
    const frame=pipe.process(payload,2437,false,{...reading,requestedMHz:69});
    assert(!pipe.baseline && frame.correctedUnit==="dBFS");
    assert(Math.abs(frame.status.confidenceLower+27.2)<1e-6);
    pipe.startCalibration(8);
    pipe.process(payload,2437,true,{...reading,estimatedHz:40000000});
    assert(!pipe.calibration && !pipe.baseline);
  });
  test("Absolute RF grid includes the last valid FFT bin and marks missing coverage", () => {
    const pipe = new Pipeline(validateConfig(2397, 2477, 1024), { diversity: true });
    const source = new Float64Array(1024).fill(1);
    assert(pipe.toGrid(source, 2437, 80 / 1024)[1023] === 1);
    assert(Number.isNaN(pipe.toGrid(source, 2439, 80 / 1024)[0]));
    assert(Number.isNaN(pipe.combine([new Float64Array(1024).fill(NaN), source, source])[0]));
  });
  for (const window of ["hann", "blackman-harris"]) {
    for (const n of [1024, 2048, 4096, 8192, 16384]) test(`Welch PSD ${window} ${n}: integrated density equals tone power`, () => {
      const pipe = new Pipeline(config(n), { mode: "welch", window, spur: false });
      const payload = signal(n, { tones: [{ bin: n / 16, amplitude: 160 }] });
      const density = pipe.welchPower(payload);
      const total = density.reduce((sum, value) => sum + value, 0) * 80e6 / density.length;
      near(10 * Math.log10(total), 20 * Math.log10(160 / 512), 0.04);
      const frame = pipe.process(payload, 2437);
      assert(frame.correctedUnit === "dBFS/Hz");
      near(frame.resolutionHz, 80e6 / density.length);
      assert(pipe.welchPower(signal(n, { dcI: 100, dcQ: -70 })).every(value => value === 0));
    });
  }
  test("Welch baseline is measured at Welch resolution and remains consistent across modes", () => {
    const pipe = new Pipeline(config(), { mode: "welch", spur: false });
    pipe.startCalibration(8);
    const payload = signal(4096, { noise: 30 });
    let frame;
    for (let i = 0; i < 8; i++) frame = pipe.process(payload, 2437);
    for (const value of frame.corrected) if (Number.isFinite(value)) near(value, 0, 0.001);
    pipe.setOptions({ mode: "instant" });
    frame = pipe.process(payload, 2437);
    for (const value of frame.corrected) if (Number.isFinite(value)) near(value, 0, 0.001);
  });
  test("Blackman-Harris coherent gain preserves a bin-centered tone amplitude", () => {
    const spec = new Spectrum(validateConfig(2397, 2477, 4096), "blackman-harris");
    const values = spec.compute(signal(4096, { tones: [{ bin: -256, amplitude: 200 }] }));
    near(values[2048 - 256], 20 * Math.log10(200 / 512), 0.02);
  });
  let failures = 0;
  for (const { name, fn } of tests) {
    try { await fn(); report(`PASS ${name}`); }
    catch (error) { failures++; report(`FAIL ${name}: ${error.stack || error.message}`); }
  }
  return { total: tests.length, failures };
}
