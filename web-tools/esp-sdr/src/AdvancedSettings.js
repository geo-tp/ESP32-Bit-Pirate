// SPDX-License-Identifier: MIT
export function setupAdvancedSettings(details) {
  const tabs = [...details.querySelectorAll("[data-settings-tab]")];
  const panels = [...details.querySelectorAll("[data-settings-panel]")];

  function select(section, focus = false) {
    const selected = tabs.find(tab => tab.dataset.settingsTab === section);
    if (!selected || selected.disabled) return;
    for (const tab of tabs) {
      tab.setAttribute("aria-selected", String(tab === selected));
      tab.tabIndex = tab === selected ? 0 : -1;
    }
    for (const panel of panels) panel.hidden = panel.dataset.settingsPanel !== section;
    if (focus) selected.focus();
  }

  for (const tab of tabs) {
    tab.addEventListener("click", () => select(tab.dataset.settingsTab));
    tab.addEventListener("keydown", event => {
      if (!["ArrowLeft", "ArrowRight", "Home", "End"].includes(event.key)) return;
      event.preventDefault();
      const available = tabs.filter(item => !item.disabled);
      const index = available.indexOf(tab);
      const next = event.key === "Home" ? 0 : event.key === "End" ? available.length - 1
        : (index + (event.key === "ArrowRight" ? 1 : -1) + available.length) % available.length;
      select(available[next].dataset.settingsTab, true);
    });
  }
  details.querySelector("[data-settings-close]").addEventListener("click", () => {
    details.open = false;
    details.querySelector("summary").focus();
  });

  return {
    select,
    setCalibrating(active) {
      // Keep the antenna instructions available until the guided flow is finished.
      for (const tab of tabs) {
        tab.disabled = active && tab.dataset.settingsTab !== "calibration";
        tab.title = tab.disabled ? "Finish or cancel calibration to change other settings." : "";
      }
      if (active) select("calibration");
    }
  };
}
