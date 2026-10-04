import { planSweep, SweepPipeline, localNoise, signalRegions } from "../src/Sweep.js";
import { aggregateBins } from "../src/Aggregation.js";
import { SpectrumSmoother } from "../src/SpectrumSmoother.js";
import { SdrClient } from "../src/SdrClient.js";
import { FakePort, makeFrame } from "./protocol-tests.js";
import { signal } from "./pipeline-tests.js";

const assert = (value, message = "Assertion failed") => { if (!value) throw new Error(message); };
const near = (actual, expected, tolerance) => assert(Math.abs(actual - expected) <= tolerance, `${actual} != ${expected}`);
function capture(plan, sweepId, sliceIndex, captureIndex, amplitude = 160, frequency = 2420) {
  const centerMHz = plan.centers[sliceIndex];
  return { centerMHz, sweepId, sliceIndex, captureIndex, payload: signal(plan.sampleCount, {
    noise: 20, seed: 1 + sweepId * 1000 + sliceIndex * 17 + captureIndex,
    tones: Math.abs(frequency - centerMHz) <= plan.sliceHalfMHz ? [{ bin: (frequency - centerMHz) / 80 * plan.sampleCount, amplitude }] : []
  }) };
}
function sweep(pipe, id, amplitude = 160) {
  let result;
  for (let i = 0; i < pipe.plan.centers.length; i++) for (let j = 0; j < pipe.plan.dwellFrames; j++) result = pipe.process(capture(pipe.plan, id, i, j, amplitude));
  return result;
}

export async function runSweepTests(report = () => {}) {
  const tests = [], test = (name, fn) => tests.push({ name, fn });
  test("Spectrum smoothing has the same linear-power response at different sweep rates", () => {
    for (const step of [5, 20, 100, 500]) {
      const smoother = new SpectrumSmoother(500);
      near(smoother.update(new Float32Array([-60]), 0)[0], -60, 0.0001);
      let result;
      for (let time = step; time <= 500; time += step) result = smoother.update(new Float32Array([-20]), time);
      const expectedPower = 1e-6 * Math.exp(-1) + 1e-2 * (1 - Math.exp(-1));
      near(result[0], 10 * Math.log10(expectedPower), 0.0001);
      // A slow sweep should show the new observation without many sweeps of extra lag.
      near(smoother.update(new Float32Array([-20]), 5500)[0], -20, 0.001);
    }
  });
  test("Smoothing reduces fast fluctuations and keeps gaps blank without stale signals", () => {
    const smoother = new SpectrumSmoother(500);
    smoother.update(new Float32Array([-60, -20]), 0);
    const values = [];
    for (let time = 10; time <= 1000; time += 10) {
      const result = smoother.update(new Float32Array([time % 20 ? -20 : -60, NaN]), time);
      if (time > 500) values.push(result[0]);
      assert(Number.isNaN(result[1]));
    }
    assert(Math.max(...values) - Math.min(...values) < 2, "40 dB flicker should become a steady trace");
    const restored = smoother.update(new Float32Array([-20, -80]), 1010);
    near(restored[1], -80, 0.0001);
    near(smoother.update(new Float32Array([-45]), 1020)[0], -45, 0.0001);
    smoother.reset();
    near(smoother.update(new Float32Array([-90]), 1030)[0], -90, 0.0001);
  });
  test("Instant response and returned buffers never modify smoothing state or input", () => {
    const smoother = new SpectrumSmoother(0), input = new Float32Array([-30, NaN]);
    const first = smoother.update(input, 0);
    assert(first !== input && first[0] === input[0] && Number.isNaN(first[1]));
    first.fill(100); // The worker transfers this output buffer; its accumulator must be separate.
    near(smoother.update(new Float32Array([-80]), 1)[0], -80, 0.0001);
    smoother.setResponse(1000);
    near(smoother.update(new Float32Array([-40]), 2)[0], -40, 0.0001);
    for (const value of [-1, NaN, Infinity]) {
      let failed = false; try { smoother.setResponse(value); } catch { failed = true; }
      assert(failed);
    }
  });
  test("Display smoothing preserves sweep statistics, waterfall samples and calibration resets", () => {
    const plan = planSweep(2415,2425,"fast");
    const smooth = new SweepPipeline(plan), instant = new SweepPipeline(plan, { smoothingMs: 0 });
    for (let id = 0; id < 3; id++) {
      const a = sweep(smooth, id, id % 2 ? 0 : 160).frame;
      const b = sweep(instant, id, id % 2 ? 0 : 160).frame;
      for (const key of ["current", "average", "peak", "occupancy", "waterfall", "waterfallMedian"]) {
        assert(a[key].every((value, i) => Object.is(value, b[key][i])), `${key} changed with display smoothing`);
      }
      assert(b.smoothed.every((value, i) => Object.is(value, b.current[i])));
      if (id === 1) assert(a.smoothed.some((value, i) => value > a.current[i] + 1));
    }
    smooth.smoother.setResponse(1000);
    assert(smooth.sweeps === 3 && smooth.visits.every(count => count === 3));
    const frame = sweep(smooth, 3).frame;
    assert(frame.smoothed.every((value, i) => Object.is(value, frame.current[i])));
    for (const reset of [() => smooth.reset(), () => smooth.startCalibration(), () => smooth.clearCalibration(), () => smooth.setCleanup(true)]) {
      smooth.smoother.update(new Float32Array([-20]), 0);
      reset();
      assert(smooth.smoother.power === null, "Old levels must not cross history or reference changes");
    }
  });
  test("Sweep plans cover the requested range with reliable overlapping slices", () => {
    for (const profile of ["fast", "balanced", "sensitive"]) for (const [start, end] of [[2200,2800], [2400,2484], [2200,2202], [2798,2800], [2440,2443]]) {
      const plan = planSweep(start, end, profile);
      assert(plan.centers[0] - plan.sliceHalfMHz <= start && plan.centers.at(-1) + plan.sliceHalfMHz >= end);
      assert(plan.centers.every(Number.isInteger));
      for (let i = 1; i < plan.centers.length; i++) assert(plan.centers[i] - plan.centers[i - 1] <= plan.stepMHz);
      assert(plan.spanMHz / plan.binMHz <= 65536);
    }
  });
  test("Scan plans respect advertised tuning limits and sample capacity", () => {
    const info = { center_min_mhz: 2300, center_max_mhz: 2500, max_samples: 2048 };
    const plan = planSweep(2300, 2500, "sensitive", info);
    assert(plan.sampleCount <= 2048 && plan.centers.every(c => c >= 2300 && c <= 2500));
    for (const args of [[2299,2400,"fast",info], [2400,2401], [2450,2400], [99,6001], [2400,2402,"unknown"], [NaN,2484]]) {
      let failed = false; try { planSweep(...args); } catch { failed = true; } assert(failed);
    }
  });
  test("Stitching preserves an RF tone across overlapping centers and keeps full coverage", () => {
    const pipe = new SweepPipeline(planSweep(2400,2460));
    const { frame } = sweep(pipe, 0);
    assert(frame && frame.current.every(Number.isFinite));
    const peak = frame.current.indexOf(Math.max(...frame.current));
    near(frame.firstMHz + peak * frame.binMHz, 2420, frame.binMHz * 2);
    assert(frame.current[peak] > -16 && frame.current[peak] < -8);
    assert(frame.regions.some(region => Math.abs(region.peakMHz - 2420) <= frame.binMHz));
  });
  test("Only complete sweeps create rows; dropped captures cannot reuse stale slices", () => {
    const pipe = new SweepPipeline(planSweep(2400,2460,"sensitive"));
    const p = pipe.plan;
    for (let i = 0; i < p.centers.length; i++) for (let j = 0; j < p.dwellFrames; j++) {
      if (i === 1 && j === 1) continue;
      assert(!pipe.process(capture(p, 0, i, j))?.frame);
    }
    const result = sweep(pipe, 1);
    assert(result.frame.sweeps === 1 && result.skippedSweeps === 1);
    assert(pipe.process(capture(p, 0, 0, 0)) === null);
  });
  test("Swept average uses linear power; peaks and occupancy persist across complete visits", () => {
    const pipe = new SweepPipeline(planSweep(2400,2460));
    const a = sweep(pipe, 0, 200).frame, b = sweep(pipe, 1, 0).frame;
    const i = a.current.indexOf(Math.max(...a.current));
    near(10 ** (b.average[i] / 10), (10 ** (a.current[i] / 10) + 10 ** (b.current[i] / 10)) / 2, 1e-6);
    near(b.peak[i], a.current[i], 1e-4); near(b.occupancy[i], 50, 0.001);
    pipe.reset(); const fresh = sweep(pipe, 2, 0).frame;
    assert(fresh.sweeps === 1 && fresh.peak[i] < a.current[i] - 20);
  });
  test("Calibration collects independent references at every center, without publishing mixed rows", () => {
    const pipe = new SweepPipeline(planSweep(2400,2440,"fast"));
    pipe.startCalibration();
    const p = pipe.plan;
    for (let id = 0; id < 64; id++) for (let i = 0; i < p.centers.length; i++) {
      const data = capture(p,id,i,0,0);
      data.payload = new Uint8Array(p.sampleCount * 4);
      new DataView(data.payload.buffer).setUint32(p.sampleCount / 2 * 4, 80 + i * 80, true);
      assert(!pipe.process({ ...data, retuned: true }));
      const result = pipe.process(data); assert(!result.frame);
    }
    assert(pipe.calibrationStatus().state === "ready");
    assert(pipe.calibrationStatus().count === p.centers.length * 64);
    let result;
    for (let i = 0; i < p.centers.length; i++) {
      const data = capture(p,64,i,0,0); data.payload = new Uint8Array(p.sampleCount * 4);
      new DataView(data.payload.buffer).setUint32(p.sampleCount / 2 * 4, 80 + i * 80, true);
      result = pipe.process(data);
    }
    assert(result.frame.unit === "dB above reference");
    for (const value of result.frame.current) if (Number.isFinite(value)) near(value, 0, 0.001);
    pipe.clearCalibration(); assert(sweep(pipe,65).frame.unit === "dBFS");
  });
  test("Missing calibrated coverage stays blank and does not count as an occupancy observation", () => {
    const pipe = new SweepPipeline(planSweep(2400,2440,"fast"));
    const result = sweep(pipe, 0).frame;
    assert(result.current.every(Number.isFinite));
    for (const entry of pipe.entries.values()) entry.power.fill(NaN);
    const blank = pipe.finish();
    assert(blank.current.every(Number.isNaN));
    assert(pipe.visits.every(n => n === 1));
  });
  test("Noise-relative activity follows a varying local noise floor", () => {
    const levels = Float32Array.from({ length: 2000 }, (_, i) => i < 1000 ? -85 : -55);
    levels[400] = -55; levels[1600] = -25;
    const noise = localNoise(levels, 0.02), regions = signalRegions(levels, noise, 2400, 0.02);
    near(noise[400], -85, 0.001); near(noise[1600], -55, 0.001);
    assert(regions.some(r => r.peakMHz === 2408) && regions.some(r => r.peakMHz === 2432));
  });
  test("Waterfall mean/median reduce isolated bins while spectrum aggregation preserves peaks", () => {
    const levels = new Float32Array([-90,-90,0,-90,NaN,NaN,NaN,NaN]);
    near(aggregateBins(levels,2,"peak")[0],0,0.001);
    near(aggregateBins(levels,2,"mean")[0],-6.0206,0.001);
    near(aggregateBins(levels,2,"median")[0],-90,0.001);
    assert(Number.isNaN(aggregateBins(levels,2,"mean")[1]));
  });
  test("Sweep transport tags cycles and ignores buffered old-center frames after TUNE", async () => {
    const plan = planSweep(2400,2440,"fast");
    let sequence = 0, center = plan.centers[0];
    const port = new FakePort({ onWrite(command, device) {
      const feed = (flags = 0) => device.feed(makeFrame({ flags, centerHz:center*1e6, sequence:sequence++, count:plan.sampleCount }));
      if (command.startsWith("STREAM")) { feed(1); feed(); }
      if (command.startsWith("TUNE")) { feed(); feed(); center = Number(command.split(" ")[1]); feed(1); feed(); }
      if (command === "STOP") device.feed(makeFrame({ type:2 }));
    }});
    const client = new SdrClient(); await client.connect(port); const frames = [];
    await client.sweep(plan, frame => {
      if (frame.sweepValid) {
        frames.push([frame.sweepId,frame.sliceIndex,frame.captureIndex]);
        if (frame.sweepId === 1 && frame.sliceIndex === plan.centers.length-1) void client.stop();
      }
    });
    assert(frames.length === plan.centers.length * 2);
    for (const [i, frame] of frames.entries()) assert(JSON.stringify(frame) === JSON.stringify([Math.floor(i/plan.centers.length),i%plan.centers.length,0]));
    assert(port.commands.filter(c => c === "STOP").length === 1);
    await client.disconnect();
  });
  test("A single-center scan still creates distinct sweep IDs", async () => {
    const plan = planSweep(2430,2440,"fast");
    const port = new FakePort({ onWrite(command,device) {
      if (command.startsWith("STREAM")) for (let i=0;i<4;i++) device.feed(makeFrame({ centerHz:plan.centers[0]*1e6,sequence:i,count:plan.sampleCount }));
      if (command === "STOP") device.feed(makeFrame({type:2}));
    }});
    const client = new SdrClient(); await client.connect(port); const ids=[];
    await client.sweep(plan, frame => { if (frame.sweepValid) { ids.push(frame.sweepId); if (ids.length===4) void client.stop(); } });
    assert(JSON.stringify(ids)==="[0,1,2,3]"); await client.disconnect();
  });
  test("Wide RX reduces a 600 MHz overview to at most 14 KiB of useful IQ per sweep", () => {
    const plan = planSweep(2200,2800,"fast",{}, {estimatedHz:69000000});
    assert(plan.centers.length * plan.sampleCount * 4 * plan.dwellFrames <= 14336);
    assert(plan.spanMHz / plan.binMHz <= 600);
    const pipe = new SweepPipeline(plan);
    const frame = sweep(pipe,0).frame;
    assert(frame.current.length === 600 && frame.current.every(Number.isFinite));
    assert(frame.waterfall.length === frame.current.length);
  });
  test("Every RX width covers fractional recommended and experimental ranges without gaps", () => {
    for (const width of [13,24,40,60,68.5,69]) for (const profile of ["fast","balanced","sensitive"]) for (const [a,b] of [[2400,2483.5],[2200,2800],[2200,2202],[2798,2800]]) {
      const plan=planSweep(a,b,profile,{}, {estimatedHz:width*1e6});
      assert(plan.centers[0]-plan.sliceHalfMHz<=a && plan.centers.at(-1)+plan.sliceHalfMHz>=b);
      const frame=sweep(new SweepPipeline(plan),0).frame;
      assert(frame.current.every(Number.isFinite), `Missing coverage at ${width} MHz / ${profile} / ${a}–${b}`);
    }
    assert(planSweep(2200,2800,"fast",{}, {estimatedHz:69000000}).centers.length < planSweep(2200,2800).centers.length);
  });
  test("Narrower actual readback leaves gaps; filter changes invalidate scan references and occupancy", () => {
    const plan=planSweep(2400,2460,"fast",{}, {estimatedHz:69000000}), pipe=new SweepPipeline(plan);
    let result;
    for(let i=0;i<plan.centers.length;i++) result=pipe.process({...capture(plan,0,i,0),bandwidth:{estimatedHz:13000000,requestedMHz:13,iCode:60,qCode:60}});
    assert(result.frame.current.some(Number.isNaN));
    assert(pipe.visits.some(n=>n===0));
    const changed=pipe.process({...capture(plan,1,0,0),bandwidth:{estimatedHz:40000000,requestedMHz:40,iCode:20,qCode:20}});
    assert(changed.bandwidthChanged && pipe.visits.every(n=>n===0));
  });
  test("Swept cleanup works without a noise reference and clears incompatible peak/occupancy history", () => {
    const pipe=new SweepPipeline(planSweep(2400,2460));
    const before=sweep(pipe,0).frame;
    pipe.setCleanup(true);
    assert(pipe.visits.every(n=>n===0) && pipe.calibrationStatus().state === "none");
    for(let id=1;id<=16;id++) {
      const result=sweep(pipe,id);
      assert(!result.frame && pipe.visits.every(n=>n===0), "Reference captures cannot enter occupancy or history");
    }
    assert(pipe.cleanupStatus().state==="ready" && pipe.calibrationStatus().state==="none");
    const after=sweep(pipe,17).frame;
    assert(after.sweeps===1 && after.unit==="dB above reference");
    const index=Math.round((2420-after.firstMHz)/after.binMHz);
    assert(after.current[index]<8, "The learned RF signal is part of the live background");
    assert(after.peak.every((p,i)=>Object.is(p,after.current[i])));
    pipe.setCleanup(false);
    const restored=sweep(pipe,18).frame;
    assert(restored.unit==="dBFS");
    near(Math.max(...restored.current),Math.max(...before.current),1);
  });
  test("Wide stitching keeps channel 1 and 10 at their RF frequencies with S3 wire IQ", () => {
    for(const profile of ["fast","balanced","sensitive"]) for(const rf of [2412,2457]) for(const [start,end] of [[2400,2483.5],[2200,2800]]) {
      // Avoid a pure test carrier exactly at the LO: DC removal intentionally removes it.
      const plan=planSweep(start,end,profile,{}, {estimatedHz:69000000});
      const pipe=new SweepPipeline(plan); let result;
      for(let i=0;i<plan.centers.length;i++) for(let j=0;j<plan.dwellFrames;j++) result=pipe.process(capture(plan,0,i,j,160,rf));
      const frame=result.frame, index=frame.current.indexOf(Math.max(...frame.current));
      near(frame.firstMHz+index*frame.binMHz,rf,plan.binMHz);
    }
  });
  let failures = 0;
  for (const {name,fn} of tests) {
    try { await fn(); report(`PASS ${name}`); } catch(error) { failures++; report(`FAIL ${name}: ${error.stack || error.message}`); }
  }
  return { total:tests.length,failures };
}
