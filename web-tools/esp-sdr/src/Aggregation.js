// SPDX-License-Identifier: MIT
// Spectrum peaks and waterfall brightness serve different purposes.
export function aggregateBins(levels, width, method = "peak") {
  const output = new Float32Array(width).fill(NaN);
  for (let x = 0; x < width; x++) {
    const start = Math.floor(x * levels.length / width), end = Math.max(start + 1, Math.floor((x + 1) * levels.length / width));
    let sum = 0, count = 0, peak = -Infinity;
    const values = [];
    for (let i = start; i < end; i++) if (Number.isFinite(levels[i])) {
      peak = Math.max(peak, levels[i]);
      if (method === "mean") sum += 10 ** (levels[i] / 10);
      if (method === "median") values.push(levels[i]);
      count++;
    }
    if (!count) continue;
    if (method === "mean") output[x] = 10 * Math.log10(Math.max(1e-20, sum / count));
    else if (method === "median") { values.sort((a, b) => a - b); output[x] = values[count >> 1]; }
    else output[x] = peak;
  }
  return output;
}
