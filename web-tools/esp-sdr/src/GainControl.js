// SPDX-License-Identifier: MIT
export function updateGainControl(mode, index) {
  const control = mode.closest(".gain-control");
  const manual = mode.value === "MANUAL";
  const min = Number(index.min), max = Number(index.max);
  control.dataset.manual = String(manual);
  control.style.setProperty("--gain-level", `${max > min ? (Number(index.value) - min) / (max - min) * 100 : 0}%`);
  control.querySelector("output").value = manual ? index.value : "AGC";
  index.setAttribute("aria-valuetext", manual ? `Hardware index ${index.value} of ${max}` : "Automatic gain");
  control.title = mode.disabled
    ? "Gain is unavailable while capturing or when the adapter does not support gain control."
    : manual ? `Manual gain · hardware index ${index.value} / ${max} (not dB). Applied on Start.`
    : "Automatic gain (AGC). Select Manual to adjust receiver gain with the slider.";
}
