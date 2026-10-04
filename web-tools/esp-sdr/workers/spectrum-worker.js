// SPDX-License-Identifier: MIT
import { Pipeline } from "../src/Pipeline.js";

let pipeline = null, options = {};

self.onmessage = ({ data }) => {
  try {
    if (data.config) {
      if (pipeline) pipeline.setConfig(data.config);
      else pipeline = new Pipeline(data.config, options);
      return;
    }
    if (data.options) {
      options = { ...options, ...data.options };
      pipeline?.setOptions(data.options);
      return;
    }
    if (data.baseline) { pipeline.startCalibration(data.baseline); return; }
    if (data.clearBaseline) { pipeline.clearBaseline(); return; }
    if (data.reset) { pipeline.reset(); return; }
    const frame = pipeline.process(data.payload, data.centerMHz, data.retuned, data.bandwidth);
    if (!frame) { self.postMessage({ skipped: true, generation: data.generation }); return; }
    self.postMessage({ ...frame, generation: data.generation, sequence: data.sequence, captureUs: data.captureUs, centerMHz: data.centerMHz },
      [frame.raw.buffer, frame.dc.buffer, frame.corrected.buffer]);
  } catch (error) {
    self.postMessage({ error: error.message });
  }
};
