// SPDX-License-Identifier: MIT
import { SweepPipeline } from "../src/Sweep.js";
let pipeline;
self.onmessage = ({ data }) => {
  try {
    if (data.plan) { pipeline = new SweepPipeline(data.plan, data.options); return; }
    if (data.smoothingMs !== undefined) { pipeline.smoother.setResponse(data.smoothingMs); return; }
    if (typeof data.cleanup === "boolean") { pipeline.setCleanup(data.cleanup); return; }
    if (data.calibrate) {
      if (data.clean) for (const pipe of pipeline.pipes) pipe.setOptions({ spur: true });
      pipeline.startCalibration(); return;
    }
    if (data.clearCalibration) { pipeline.clearCalibration(); return; }
    if (data.reset) { pipeline.reset(); return; }
    const result = pipeline.process(data);
    const buffers = result?.frame ? ["current", "smoothed", "average", "peak", "occupancy", "waterfall", "waterfallMedian"].map(key => result.frame[key].buffer) : [];
    self.postMessage({ ...result, generation: data.generation, sweepId: data.sweepId, sliceIndex: data.sliceIndex }, buffers);
  } catch (error) { self.postMessage({ error: error.message }); }
};
