#!/usr/bin/env python3
"""Browser integration and NumPy parity. Requires a local server, Chrome CDP, numpy, websocket-client."""
import argparse, base64, json, sys, time, urllib.request, websocket
sys.dont_write_bytecode = True
from pathlib import Path
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--debug-port', type=int, default=9354)
parser.add_argument('--url', default='http://127.0.0.1:8114/web-tools/esp-sdr/')
args = parser.parse_args()
base_url = args.url.rstrip('/') + '/'
debug_url = f'http://localhost:{args.debug_port}'
pages = json.load(urllib.request.urlopen(debug_url + '/json'))
page = next(p for p in pages if p['type'] == 'page')
ws = websocket.create_connection(page['webSocketDebuggerUrl'], origin=debug_url, timeout=30)
seq = 0
errors = []
def call(method, params=None):
    global seq
    seq += 1
    ws.send(json.dumps({'id': seq, 'method': method, 'params': params or {}}))
    while True:
        r = json.loads(ws.recv())
        if r.get('method') == 'Runtime.exceptionThrown': errors.append(r['params'])
        if r.get('id') == seq:
            if 'error' in r: raise RuntimeError(r['error'])
            return r.get('result', {})
def js(expression):
    r = call('Runtime.evaluate', {'expression': expression, 'returnByValue': True, 'awaitPromise': True})
    if 'exceptionDetails' in r: raise RuntimeError(r['exceptionDetails'])
    return r.get('result', {}).get('value')
def until(expression):
    for _ in range(300):
        if js(expression): return
        time.sleep(.05)
    raise AssertionError('Timeout: ' + expression)
def shot(name):
    Path('/tmp/' + name + '.png').write_bytes(base64.b64decode(call('Page.captureScreenshot', {'format':'png','captureBeyondViewport':True})['data']))
call('Runtime.enable')
call('Page.enable')
call('Emulation.setDeviceMetricsOverride', {'width':1440,'height':1000,'deviceScaleFactor':1,'mobile':False})
call('Network.enable')
call('Network.setCacheDisabled', {'cacheDisabled': True})
call('Network.clearBrowserCache')
js('window.testResult = undefined')
run = str(time.time_ns())
call('Page.navigate', {'url': base_url + 'tests/?run=' + run})
until('location.search === ' + json.dumps('?run=' + run))
time.sleep(.2)
until('window.testResult !== undefined')
print(js('document.querySelector("#results").textContent'))
assert js('window.testResult.failures') == 0
assert js('window.testResult.total') >= 62
call('Page.navigate', {'url': base_url})
until('location.href === ' + json.dumps(base_url) + ' && document.readyState === "complete" && !!document.querySelector("#startButton")')
js('document.documentElement.dataset.theme = "dark"')
assert js('!document.querySelector("#advancedSettings").open && !document.querySelector("#rawOverlay").checked && document.querySelector("#autoScale").checked')
assert js('document.querySelector("#liveGainMode").value === "MANUAL" && document.querySelector("#liveGainIndex").value === document.querySelector("#liveGainIndex").max && document.querySelector("output[for=liveGainIndex]").value === "82"')
assert js('document.querySelector("#scanGainMode").value === "MANUAL" && document.querySelector("#scanGainIndex").value === "41" && document.querySelector("output[for=scanGainIndex]").value === "41"')
assert js('document.querySelector("#calibrationGuide").hidden && document.querySelector("#calibrateButton").disabled')
assert js('document.querySelector("#cleanButton").parentElement === document.querySelector("#startButton").parentElement && document.querySelector("#scanCleanButton").parentElement === document.querySelector("#scanStart").parentElement')
assert js('!document.querySelector("#scanRecommended, #scanExperimental, #scanProgress, #scanStatus, .scan-progress")')
assert js('["rxSummary","liveRegionHint","streamStats"].every(id=>document.getElementById(id).closest("#advancedSettings"))')
assert js('Boolean(document.querySelector("#scanRegionHint").closest(".wide-spectrum") && document.querySelector("#scanRxSummary").closest("#scanAdvanced"))')
assert js('document.querySelectorAll(".action-button svg").length===6')

js('''(async () => {
 const {FakePort, makeFrame, infoLine} = await import('./tests/protocol-tests.js');
 const {signal} = await import('./tests/pipeline-tests.js');
 const {Wideband} = await import('./src/Wideband.js');
 const accept = Wideband.prototype.accept;
 Wideband.prototype.accept = function(data) { window.scanForTest = this; return accept.call(this, data); };
 window.testPorts = [];
 window.noiseOnly = false;
 Object.defineProperty(navigator.serial, 'requestPort', {configurable: true, value: async () => {
   const port = new FakePort({fragment: 8191, info: infoLine.replace("center_min_mhz=2200 center_max_mhz=2800", "center_min_mhz=100 center_max_mhz=6000") + " bw_control=1 bw_min_mhz=13 bw_max_mhz=69 bw_requested_mhz=-1 frame_versions=1,2,3", onWrite(command, device) {
     if (command.startsWith('RATE ')) {
       device.rate = Number(command.split(' ')[1]) * 1e6;
       device.feed(new TextEncoder().encode(`RATE fs_hz=${device.rate} supported_hz=80000000,40000000,16000000 nominal=1\\nEND\\n`));
     }
     if (command.startsWith('GAIN ')) {
       const [,mode,index] = command.split(' ');
       device.feed(new TextEncoder().encode(`GAIN supported=1 mode=${mode} index=${mode==='MANUAL'?index:-1} max=82 calibrated_db=0\\nEND\\n`));
     }
     if (command.startsWith('BANDWIDTH')) {
       if(command !== 'BANDWIDTH?') {
         const arg=command.split(' ')[1]; device.requested=arg==='WIDE'?0:arg==='AUTO'?-1:Number(arg);
         device.width=device.requested===0?68.5:device.requested===-1?33.5:device.requested;
       }
       device.feed(new TextEncoder().encode(`BANDWIDTH requested_mhz=${device.requested??-1} target_hz=${(device.width??0)*1e6} estimated_hz=24000000 dcap_i=20 dcap_q=20 actual_valid=1 approximate=1\\nEND\\n`));
     }
     if (command.startsWith('STREAM')) {
       const fields = command.split(' ');
       device.center = Number(fields[1]); device.samples = Number(fields[2]); device.sequence = 0;
       device.retuned = true;
       const feed = () => {
         const n = device.samples;
         const payload = signal(n, {noise: 30, seed: device.sequence + 1,
           tones: window.noiseOnly ? [] : [...(Math.abs((window.rfMHz ?? 2442) - device.center) <= (device.width ?? 24) / 2 ? [{bin: ((window.rfMHz ?? 2442) - device.center) / 80 * n, amplitude: 160}] : []), {bin: -5 / 80 * n, amplitude: 50}]});
         if (window.narrowNoise) {
           // A five-sample moving average simulates a receiver with narrower noise bandwidth.
           const input = new DataView(payload.slice().buffer), output = new DataView(payload.buffer);
           for (let i = 0; i < n; i++) {
             let real = 0, imag = 0;
             for (let k = 0; k < 5; k++) {
               const word = input.getUint32((i + k) % n * 4, true);
               const a = word & 1023, b = (word >>> 10) & 1023;
               real += a >= 512 ? a - 1024 : a;
               imag += b >= 512 ? b - 1024 : b;
             }
             output.setUint32(i * 4, (Math.round(real / 5) & 1023) | ((Math.round(imag / 5) & 1023) << 10), true);
           }
         }
         device.feed(makeFrame({version:2, requestedMHz:device.requested??-1, estimatedHz:(window.diverseBandwidth && device.center===2435 ? 36 : device.width??24)*1e6, sequence: device.sequence++, centerHz: device.center * 1e6,
           flags: device.retuned ? 1 : 0, count: n, payload}));
         device.retuned = false;
       };
       feed(); device.timer = setInterval(feed, 8);
     }
     if (command.startsWith('TUNE')) { device.center = Number(command.split(' ')[1]); device.retuned = true; }
     if (command === 'STOP') {clearInterval(device.timer); device.feed(makeFrame({version:2,type: 2,estimatedHz:0}));}
   }});
   const close = port.close.bind(port);
   port.close = async () => {clearInterval(port.timer); await close();};
   testPorts.push(port); return port;
 }});
})()''')
js('document.querySelector("#connectButton").click()')
until('document.querySelector("#connectionStatus").textContent === "Connected"')
assert js('document.querySelector("#antennaHint").textContent === "ESP SDR Adapter · BPRF1" && document.querySelector("#antennaHint").dataset.state === "connected" && !document.querySelector("#antennaHint").hidden')
# Live always exposes the full firmware range; Wideband keeps a separate opt-in range.
assert js('document.querySelector("#centerFreq").min === "100" && document.querySelector("#centerFreq").max === "6000"')
assert js('document.querySelector("#scanFrom").min === "2200" && document.querySelector("#scanTo").max === "2800" && !document.querySelector("#scanExtendedRange").checked')
assert js('document.querySelector("#scanExtendedRange").closest("#scanAdvanced") !== null && document.querySelector("#scanExtendedRange").closest("#scanRangeField") === null')
assert js('document.querySelector("#scanBandwidth").tagName === "SELECT" && [...document.querySelector("#scanBandwidth").options].map(o=>o.value).join(",") === "WIDE,AUTO"')
js('document.querySelector("#scanExtendedRange").checked=true; document.querySelector("#scanExtendedRange").dispatchEvent(new Event("change"))')
assert js('document.querySelector("#scanFrom").min === "100" && document.querySelector("#scanTo").max === "6000" && document.querySelector("#centerFreq").min === "100" && document.querySelector("#centerFreq").max === "6000"')
js('document.querySelector("#scanExtendedRange").checked=false; document.querySelector("#scanExtendedRange").dispatchEvent(new Event("change"))')
assert js('document.querySelector("#scanFrom").min === "2200" && document.querySelector("#scanTo").max === "2800" && document.querySelector("#centerFreq").min === "100" && document.querySelector("#centerFreq").max === "6000"')
assert js('document.querySelectorAll("#widePanel .plot-click-hint").length === 2 && [...document.querySelectorAll("#widePanel .plot-click-hint")].every(el => el.textContent.includes("open it in Live SDR"))')
# Gain is available beside the frequency controls, with Advanced settings closed.
for prefix, mode, settings in [('scan', 'wide', 'scanAdvanced'), ('live', 'live', 'advancedSettings')]:
    js(f'document.getElementById("{mode}Tab").click()')
    until(f'!document.getElementById("{mode}Panel").hidden')
    assert js(f'!document.getElementById("{settings}").open && document.getElementById("{prefix}GainMode").checkVisibility()')
    assert js(f'document.getElementById("{prefix}GainMode").closest(".frequency-gain-controls") !== null')
    js(f'document.getElementById("{prefix}GainMode").value="MANUAL"; document.getElementById("{prefix}GainMode").dispatchEvent(new Event("change"))')
    assert js(f'!document.getElementById("{prefix}GainIndex").disabled && document.getElementById("{prefix}GainIndex").checkVisibility()')
    initial_gain = '82' if prefix == 'live' else '41'
    expected_status = "4,096 samples" if prefix == "live" else "Wide RX"
    assert js(f'document.getElementById("{prefix}SettingsStatus").textContent.includes("{expected_status}")')
    js(f'document.getElementById("{prefix}GainIndex").value="57"; document.getElementById("{prefix}GainIndex").dispatchEvent(new Event("input"))')
    assert js(f'document.querySelector("output[for={prefix}GainIndex]").value === "57" && !document.getElementById("{prefix}SettingsStatus").textContent.includes("gain")')
    start, stop, state = ('scanStart', 'scanStop', 'Scanning') if prefix == 'scan' else ('startButton', 'stopButton', 'Live')
    js(f'document.getElementById("{start}").click()')
    until(f'document.querySelector("#connectionStatus").textContent === "{state}" && testPorts[0].commands.includes("GAIN MANUAL 57")')
    assert js(f'document.getElementById("{prefix}GainMode").disabled && document.getElementById("{prefix}GainIndex").disabled')
    assert js('testPorts[0].commands.filter(c=>c.startsWith("GAIN ")).at(-1) === "GAIN MANUAL 57"')
    js(f'document.getElementById("{stop}").click()')
    until('document.querySelector("#connectionStatus").textContent === "Connected"')
    js(f'document.getElementById("{prefix}GainMode").value="HARDWARE"; document.getElementById("{prefix}GainMode").dispatchEvent(new Event("change"))')
    assert js(f'document.getElementById("{prefix}GainIndex").disabled && document.getElementById("{prefix}GainIndex").checkVisibility() && document.querySelector("output[for={prefix}GainIndex]").value === "AGC"')
    js(f'document.getElementById("{settings}").open=true')
    assert js(f'!document.getElementById("{prefix}ReceiverSettings").hidden && document.getElementById("{prefix}CalibrationSettings").hidden')
    js(f'document.getElementById("{prefix}ReceiverTab").focus(); document.activeElement.dispatchEvent(new KeyboardEvent("keydown",{{key:"ArrowRight",bubbles:true}}))')
    assert js(f'document.activeElement.id==="{prefix}DisplayTab" && !document.getElementById("{prefix}DisplaySettings").hidden')
    js('document.activeElement.dispatchEvent(new KeyboardEvent("keydown",{key:"End",bubbles:true}))')
    assert js(f'document.activeElement.id==="{prefix}CalibrationTab" && !document.getElementById("{prefix}CalibrationSettings").hidden')
    js('document.activeElement.dispatchEvent(new KeyboardEvent("keydown",{key:"Home",bubbles:true}))')
    assert js(f'document.activeElement.id==="{prefix}ReceiverTab"')
    js(f'document.querySelector("#{settings} [data-settings-close]").click()')
    assert js(f'!document.getElementById("{settings}").open && document.activeElement===document.querySelector("#{settings} > summary")')
# Presets follow actual per-frame widths, including quantized Wide and firmware AUTO.
js('document.querySelector("#liveTab").click()')
until('!document.querySelector("#livePanel").hidden && !document.querySelector("#startButton").disabled')
for setting, expected in [('13','13'),('WIDE','68.5'),('AUTO','33.5')]:
    js('document.querySelector("#rxExact").value=' + json.dumps(setting) + '; document.querySelector("#rxExact").dispatchEvent(new Event("input")); document.querySelector("#startButton").click()')
    until('document.querySelector("#rxSummary").textContent.includes(' + json.dumps('≈'+expected+' MHz') + ')')
    until('testPorts[0].sequence > 4')
    if setting == 'WIDE': shot('sdr-wide-live')
    js('document.querySelector("#stopButton").click()')
    until('document.querySelector("#connectionStatus").textContent === "Connected"')
js('window.stopsBeforeLive=testPorts[0].commands.filter(c=>c==="STOP").length; document.querySelector("#rxPreset").value="40"; document.querySelector("#rxPreset").dispatchEvent(new Event("change"))')
js('document.querySelector("#startButton").click()')
until('document.querySelector("#streamStats").textContent.includes("LO 2437")')
assert js('document.querySelector("#centerFreq").disabled')
until('testPorts[0].sequence > 100')
assert not js('document.querySelector("#statusMessage").classList.contains("error")')
# Reduce noise measures live input in one click: no antenna dialog or RF commands.
js('window.commandsBeforeClean=testPorts[0].commands.length; document.querySelector("#cleanButton").click()')
assert js('document.querySelector("#cleanButton").getAttribute("aria-pressed")==="true" && document.querySelector("#calibrationGuide").hidden && !document.querySelector("#advancedSettings").open')
until('document.querySelector("#processingState").textContent.includes("Live-reference cleanup ON")')
assert js('document.querySelector("#processingState").textContent.includes("Baseline not calibrated") && document.querySelector("#spectrumUnit").textContent.includes("reference") && testPorts[0].commands.length===commandsBeforeClean')
assert js('document.querySelector("#cleanButton svg") !== null && document.querySelector("#cleanButton .action-label").textContent==="Reduce noise" && document.querySelector("#cleanButton .action-state").textContent==="ON" && document.querySelector("#cleanButton").getAttribute("aria-busy")==="false"')
shot('sdr-direct-clean-live')
js('document.querySelector("#cleanButton").click()')
until('document.querySelector("#processingState").textContent.includes("Live-reference cleanup OFF")')
assert js('document.querySelector("#calibrationGuide").hidden && document.querySelector("#cleanButton .action-state").textContent==="OFF"')
# A second click cancels a partial live reference, with no antenna prompts.
js('document.querySelector("#cleanButton").click(); window.cleanBusy=document.querySelector("#cleanButton").getAttribute("aria-busy"); document.querySelector("#cleanButton").click()')
assert js('cleanBusy==="true" && document.querySelector("#cleanButton").getAttribute("aria-busy")==="false" && document.querySelector("#calibrationGuide").hidden')
until('document.querySelector("#processingState").textContent.includes("Live-reference cleanup OFF")')
# Stopping during reference collection must not leave a stuck Reducing state.
js('document.querySelector("#cleanButton").click(); document.querySelector("#stopButton").click()')
until('document.querySelector("#connectionStatus").textContent === "Connected"')
assert js('document.querySelector("#cleanButton").getAttribute("aria-pressed")==="false" && document.querySelector("#cleanButton .action-state").textContent==="OFF" && document.querySelector("#calibrationGuide").hidden')
js('window.stopsBeforeLive=testPorts[0].commands.filter(c=>c==="STOP").length; document.querySelector("#startButton").click()')
until('!document.querySelector("#calibrateButton").disabled && testPorts[0].sequence>8')
# Calibrate with actual varying noise through Web Serial -> client -> worker -> canvas.
js('window.noiseOnly = true; document.querySelector("#calibrateButton").click()')
assert js('!document.querySelector("#calibrationGuide").hidden && document.querySelector("#advancedSettings").open && document.querySelector("#calibrationGuide").closest("#advancedSettings") !== null')
assert js('document.querySelector("#liveCalibrationTab").getAttribute("aria-selected")==="true" && document.querySelector("#liveReceiverTab").disabled && document.querySelector("#liveDisplayTab").disabled')
assert js('document.querySelector("#processingState").textContent.includes("Baseline not calibrated")')
js('document.querySelector("#calibrationCancelButton").click()')
assert js('document.querySelector("#calibrationGuide").hidden')
assert js('!document.querySelector("#liveReceiverTab").disabled && !document.querySelector("#liveDisplayTab").disabled')
# Cancelling collection removes the unfinished reference and reminds us to reconnect.
js('document.querySelector("#calibrateButton").click(); document.querySelector("#calibrationStartButton").click()')
assert js('document.querySelector("#windowSelect").disabled && !document.querySelector("#calibrationProgressWrap").hidden')
js('document.querySelector("#calibrationCancelButton").click()')
assert js('document.querySelector("#calibrationTitle").textContent === "Measurement stopped"')
js('document.querySelector("#calibrationDoneButton").click()')
until('document.querySelector("#processingState").textContent.includes("Baseline not calibrated")')
# Calibration automatically selects Corrected, even when starting from a raw view.
js('document.querySelector("#spurToggle").checked=false; document.querySelector("#viewSelect").value="raw"; document.querySelector("#viewSelect").dispatchEvent(new Event("change")); document.querySelector("#calibrateButton").click(); document.querySelector("#calibrationStartButton").click()')
assert js('document.querySelector("#viewSelect").value === "corrected" && document.querySelector("#spurToggle").checked')
until('document.querySelector("#processingState").textContent.includes("Baseline calibrated")')
assert js('document.querySelector("#spectrumUnit").textContent.includes("reference")')
assert js('document.querySelector("#rangeSummary").textContent === "2417–2457 MHz · live"')
assert js('!document.querySelector("#calibrationDoneButton").hidden && !document.querySelector("#windowSelect").disabled')
assert js('document.querySelector("#calibrationTitle").textContent === "Reconnect your antenna"')
assert js('document.activeElement === document.querySelector("#calibrationDoneButton")')
shot('sdr-calibration-complete')
js('document.querySelector("#calibrationDoneButton").click()')
assert js('document.querySelector("#calibrationGuide").hidden && document.querySelector("#calibrateButton").textContent.includes("Recalibrate")')
# Cancelling preparation preserves a previously completed reference.
js('document.querySelector("#calibrateButton").click(); document.querySelector("#calibrationCancelButton").click()')
assert js('document.querySelector("#calibrationLabel").textContent.includes("Calibrated")')
# A narrower reference marks lower-confidence edges without changing the actual RX span.
js('window.narrowNoise = true; document.querySelector("#calibrateButton").click()')
call('Emulation.setDeviceMetricsOverride', {'width':390,'height':844,'deviceScaleFactor':1,'mobile':False})
js('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))')
shot('sdr-calibration-mobile')
js('document.querySelector("#calibrationStartButton").click()')
until('document.querySelector("#processingState").textContent.includes("Baseline calibrated")')
assert js('document.querySelector("#rangeSummary").textContent === "2417–2457 MHz · live"')
assert js('document.querySelector("#spectrumUnit").textContent.includes("reference")')
js('document.querySelector("#calibrationDoneButton").click()')
call('Emulation.setDeviceMetricsOverride', {'width':1440,'height':1000,'deviceScaleFactor':1,'mobile':False})
js('document.querySelector("#advancedSettings").open = true')
js('document.querySelector("#modeSelect").value="welch"; document.querySelector("#modeSelect").dispatchEvent(new Event("change"))')
until('document.querySelector("#analysisText").textContent.includes("Welch resolution 78.13")')
js('document.querySelector("#clearBaselineButton").click()')
until('document.querySelector("#spectrumUnit").textContent.includes("dBFS/Hz")')
assert js('document.querySelector("#rxSummary").textContent.includes("≈40 MHz")')
js('window.narrowNoise = false')
js('document.querySelector("#windowSelect").value="blackman-harris"; document.querySelector("#windowSelect").dispatchEvent(new Event("change"))')
until('document.querySelector("#processingState").textContent.includes("Blackman-Harris")')
# Stop while collecting cannot leave a calibrated or permanently busy UI.
js('document.querySelector("#calibrateButton").click(); document.querySelector("#calibrationStartButton").click()')
js('document.querySelector("#stopButton").click()')
until('document.querySelector("#connectionStatus").textContent === "Connected"')
assert js('document.querySelector("#calibrationTitle").textContent === "Measurement stopped" && !document.querySelector("#windowSelect").disabled')
js('document.querySelector("#calibrationDoneButton").click()')
assert js('testPorts[0].commands.filter(c => c === "STOP").length === stopsBeforeLive + 1')
# Restart and calibrate every center with tuning diversity.
js('window.diverseBandwidth=true; document.querySelector("#diversityToggle").checked = true; document.querySelector("#modeSelect").value="instant"; document.querySelector("#startButton").click()')
until('testPorts[0].commands.filter(c => c.startsWith("TUNE")).length > 8')
assert js('document.querySelector("#rangeSummary").textContent === "2419–2455 MHz · live"')
js('document.querySelector("#calibrateButton").click(); document.querySelector("#calibrationStartButton").click()')
assert js('document.querySelector("#calibrationProgress").max === 192')
until('document.querySelector("#processingState").textContent.includes("Baseline calibrated")')
assert js('document.querySelector("#analysisText").textContent.includes("64 reference captures per center")')
# Changing the window invalidates calibration, restoring the default display width.
js('document.querySelector("#calibrationDoneButton").click(); document.querySelector("#windowSelect").value="hann"; document.querySelector("#windowSelect").dispatchEvent(new Event("change")); window.noiseOnly = false')
until('document.querySelector("#processingState").textContent.includes("Baseline not calibrated")')
assert js('document.querySelector("#calibrationLabel").textContent === "Calibration · optional"')
until('testPorts[0].sequence > 340')
print('Live diversity:', js('document.querySelector("#streamStats").textContent'))
js('document.querySelector("#advancedSettings").open = false')
call('Emulation.setDeviceMetricsOverride', {'width':1440, 'height':1000, 'deviceScaleFactor':1, 'mobile':False})
js('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))')
shot('sdr-aligned-live')
# Multiple processing changes while a frame is in flight must never paint the old configuration.
js('''for (const mode of ['peak', 'welch', 'average', 'instant']) {
 document.querySelector('#modeSelect').value=mode;
 document.querySelector('#modeSelect').dispatchEvent(new Event('change'));
}''')
until('document.querySelector("#processingState").textContent.includes("Instant")')
# Clear peaks stays accessible without opening Advanced settings.
js('document.querySelector("#modeSelect").value="peak"; document.querySelector("#modeSelect").dispatchEvent(new Event("change"))')
until('document.querySelector("#processingState").textContent.includes("Peak hold")')
assert js('!document.querySelector("#peakResetButton").hidden && !document.querySelector("#peakResetButton").disabled')
js('document.querySelector("#peakResetButton").click()')
until('document.querySelector("#processingState").textContent.includes("Peak hold")')
# Wideband shares the connection and drains the live stream before changing modes.
js('window.diverseBandwidth=false; window.rfMHz = 2446; document.querySelector("#wideTab").click()')
until('!document.querySelector("#widePanel").hidden && document.querySelector("#connectionStatus").textContent === "Connected"')
assert js('document.querySelector("#livePanel").hidden && !document.querySelector("#scanAdvanced").open')
assert js('document.querySelector("#scanFrom").value === "2330" && document.querySelector("#scanTo").value === "2550" && document.querySelector("#scanProfile").value === "balanced"')
# Range changes pick a sensible detail level; Start preserves a subsequent manual choice.
js('document.querySelector("#scanFrom").value="2200"; document.querySelector("#scanFrom").dispatchEvent(new Event("input"))')
for end, expected in [(2299.5,"sensitive"),(2300,"balanced"),(2499.5,"balanced"),(2500,"fast"),(2800,"fast")]:
    js('document.querySelector("#scanTo").value='+str(end)+'; document.querySelector("#scanTo").dispatchEvent(new Event("input"))')
    assert js('document.querySelector("#scanProfile").value')==expected
js('document.querySelector("#scanFrom").value="2800"; document.querySelector("#scanFrom").dispatchEvent(new Event("input"))')
# The dual slider keeps a minimum 2 MHz range instead of allowing crossed ends.
assert js('document.querySelector("#scanFrom").value==="2798" && document.querySelector("#scanError").hidden && !document.querySelector("#scanStart").disabled')
js('document.querySelector("#scanFrom").value="2400"; document.querySelector("#scanTo").value="2484"; document.querySelector("#scanFrom").dispatchEvent(new Event("input")); document.querySelector("#scanStart").click()')
until('Number(document.querySelector("#scanWaterfall").dataset.sweeps) >= 3')
assert js('document.querySelector("#connectionStatus").textContent === "Scanning" && document.querySelector("#scanFrom").disabled && document.querySelector("#statusMessage").textContent === ""')
assert js('!document.querySelector("#scanActivity, #scanViewHint, .activity-chip")')
assert js('document.querySelector("#scanSummary").textContent.includes("84 MHz ·")')
assert js('document.querySelector("#scanProfile").value==="sensitive" && document.querySelector("#scanSummary").textContent.includes("0.25 MHz detail")')
for view in ['average', 'peak', 'occupancy', 'current']:
    js('document.querySelector(\'[data-scan-view="' + view + '"]\').click()')
    assert js('document.querySelector(\'[data-scan-view="' + view + '"]\').getAttribute("aria-pressed") === "true"')
    if view == 'occupancy':
        assert js('document.querySelector("#scanUnit").textContent.includes("% of visits") && document.querySelector(\'[data-scan-view="occupancy"]\').title.includes("not continuous airtime")')
# Wideband learns one reference per center without touching the guided calibration workflow.
js('document.querySelector("#scanCleanButton").click()')
assert js('document.querySelector("#scanCleanButton").getAttribute("aria-pressed")==="true" && document.querySelector("#scanCalibrationGuide").hidden && !document.querySelector("#scanAdvanced").open && !document.querySelector("#liveTab").disabled')
until('Number(document.querySelector("#scanWaterfall").dataset.sweeps)>=2')
assert js('document.querySelector("#scanUnit").textContent.includes("reference") && document.querySelector("#scanCleanButton .action-state").textContent==="ON" && document.querySelector("#scanCleanButton").getAttribute("aria-busy")==="false" && !document.querySelector("#scanCalibrationSummary").textContent.includes("Reference applied")')
assert js('document.querySelector("#scanCleanButton svg") !== null')
shot('sdr-direct-clean-scan')
js('document.querySelector("#scanCleanButton").click()')
until('Number(document.querySelector("#scanWaterfall").dataset.sweeps)>=2')
assert js('document.querySelector("#scanCleanButton").getAttribute("aria-pressed")==="false" && document.querySelector("#scanCleanButton .action-state").textContent==="OFF" && document.querySelector("#scanCalibrationGuide").hidden')
# Cancelling/stopping a partial scan reference also leaves no antenna workflow.
js('document.querySelector("#scanCleanButton").click(); window.scanCleanBusy=document.querySelector("#scanCleanButton").getAttribute("aria-busy"); document.querySelector("#scanCleanButton").click()')
assert js('scanCleanBusy==="true" && document.querySelector("#scanCleanButton").getAttribute("aria-busy")==="false" && document.querySelector("#scanCalibrationGuide").hidden')
js('document.querySelector("#scanCleanButton").click(); document.querySelector("#scanStop").click()')
until('document.querySelector("#connectionStatus").textContent === "Connected"')
assert js('document.querySelector("#scanCleanButton").getAttribute("aria-pressed")==="false" && document.querySelector("#scanCleanButton .action-state").textContent==="OFF" && document.querySelector("#scanCalibrationGuide").hidden')
js('document.querySelector("#scanStart").click()')
until('Number(document.querySelector("#scanWaterfall").dataset.sweeps)>=2')
js('document.querySelector("#scanCalibrate").click(); document.querySelector("#scanCalibrationStart").click()')
assert js('document.querySelector("#liveTab").disabled && document.querySelector("#scanAdvanced").open && document.querySelector("#scanCalibrationGuide").closest("#scanAdvanced") !== null')
js('document.querySelector("#scanCalibrationCancel").click()')
assert js('document.querySelector("#scanCalibrationTitle").textContent === "Calibration interrupted"')
js('document.querySelector("#scanCalibrationDone").click(); window.noiseOnly=true; document.querySelector("#scanCalibrate").click(); document.querySelector("#scanCalibrationStart").click()')
until('document.querySelector("#scanCalibrationSummary").textContent.includes("Reference applied")')
assert js('!document.querySelector("#scanCalibrationDone").hidden')
js('document.querySelector("#scanCalibrationDone").click(); window.noiseOnly=false')
until('document.querySelector("#scanUnit").textContent.includes("above reference")')
js('document.querySelector("#scanClearCalibration").click()')
until('document.querySelector("#scanUnit").textContent.includes("dBFS") && Number(document.querySelector("#scanWaterfall").dataset.sweeps) >= 2')
js('document.querySelector("#scanGuides").checked=true; document.querySelector("#scanGuides").dispatchEvent(new Event("change"))')
js('document.querySelector("#scanBleGuides").checked=true; document.querySelector("#scanBleGuides").dispatchEvent(new Event("change"))')
assert js('!document.querySelector("#scanGuides").checked && document.querySelector("#scanBleGuides").checked && !document.querySelector("#scanZigbeeGuides").checked')
js('document.querySelector("#scanZigbeeGuides").checked=true; document.querySelector("#scanZigbeeGuides").dispatchEvent(new Event("change"))')
assert js('!document.querySelector("#scanGuides").checked && !document.querySelector("#scanBleGuides").checked && document.querySelector("#scanZigbeeGuides").checked')
js('document.querySelector("#scanGuides").checked=true; document.querySelector("#scanGuides").dispatchEvent(new Event("change")); document.querySelector("#scanAdvanced").open=false')
shot('sdr-wideband-desktop')
# A spectrum click opens the same RF frequency in Live, starts streaming, and disables diversity.
js('''(() => {
  const canvas=document.querySelector('#scanSpectrum'), r=canvas.getBoundingClientRect();
  canvas.dispatchEvent(new MouseEvent('click',{bubbles:true,clientX:r.left+58+(2446-2400)/84*(r.width-76),clientY:r.top+60}));
})()''')
until('document.querySelector("#connectionStatus").textContent === "Live" && !document.querySelector("#livePanel").hidden')
assert js('Math.abs(Number(document.querySelector("#centerFreq").value)-2446) <= 1 && !document.querySelector("#diversityToggle").checked && testPorts.length === 1')
assert js('testPorts[0].commands.filter(c=>c.startsWith("STREAM")).at(-1).startsWith("STREAM2 " + document.querySelector("#centerFreq").value + " ")')
# Check the complete tuning range in both speed extremes, then preserve the last full sweep on Stop.
js('document.querySelector("#wideTab").click()')
until('!document.querySelector("#widePanel").hidden && !document.querySelector("#scanStart").disabled')
js('window.streamsBeforeCancel=testPorts[0].commands.filter(c=>c.startsWith("STREAM")).length; document.querySelector("#scanStart").click(); document.querySelector("#scanStop").click()')
until('document.querySelector("#connectionStatus").textContent === "Connected"')
assert js('testPorts[0].commands.filter(c=>c.startsWith("STREAM")).length === streamsBeforeCancel')
# Extended range remains clickable during an active scan; its range-limit change is applied after Stop.
js('document.querySelector("#scanStart").click()')
until('document.querySelector("#connectionStatus").textContent === "Scanning"')
assert js('!document.querySelector("#scanExtendedRange").disabled')
js('document.querySelector("#scanExtendedRange").checked=true; document.querySelector("#scanExtendedRange").dispatchEvent(new Event("change"))')
assert js('document.querySelector("#scanExtendedRange").checked && document.querySelector("#scanFrom").min === "2200"')
js('document.querySelector("#scanStop").click()')
until('document.querySelector("#connectionStatus").textContent === "Connected"')
assert js('document.querySelector("#scanFrom").min === "100" && document.querySelector("#scanTo").max === "6000"')
js('document.querySelector("#scanExtendedRange").checked=false; document.querySelector("#scanExtendedRange").dispatchEvent(new Event("change"))')
assert js('document.querySelector("#scanFrom").min === "2200" && document.querySelector("#scanTo").max === "2800"')
for profile in ['fast', 'balanced', 'sensitive']:
    js('document.querySelector("#scanFrom").value="2200"; document.querySelector("#scanTo").value="2800"; document.querySelector("#scanProfile").value="' + profile + '"; document.querySelector("#scanProfile").dispatchEvent(new Event("input")); document.querySelector("#scanStart").click()')
    until('Number(document.querySelector("#scanWaterfall").dataset.sweeps) >= 2')
    assert js('document.querySelector("#scanHint").textContent.includes("slower refresh")')
    assert js('document.querySelector("#scanSummary").textContent.includes("600 MHz ·")')
    if profile == 'fast':
        shot('sdr-wideband-overview-600mhz')
    js('document.querySelector("#scanStop").click()')
    until('document.querySelector("#connectionStatus").textContent === "Connected"')
    count=js('document.querySelector("#scanWaterfall").dataset.sweeps')
    js('new Promise(resolve=>setTimeout(resolve,100))')
    assert js('document.querySelector("#scanWaterfall").dataset.sweeps') == count
# A fast single-center scan smooths Current while retaining every visit in the other views.
assert js('document.querySelector("#scanSmoothing").value === "100"')
js('document.querySelector("#scanFrom").value="2440"; document.querySelector("#scanTo").value="2450"; document.querySelector("#scanFrom").dispatchEvent(new Event("input")); document.querySelector("#scanStart").click()')
until('Number(document.querySelector("#scanWaterfall").dataset.sweeps) >= 8')
assert js('scanForTest.plan.centers.length === 1 && scanForTest.latest.smoothed.some((v,i) => Math.abs(v-scanForTest.latest.current[i]) > 0.01)')
assert js('scanForTest.plots.levels(scanForTest.latest) === scanForTest.latest.smoothed')
for response in [0, 1000, 500, 100]:
    js('window.sweepsBeforeSmoothing=scanForTest.latest.sweeps; window.commandsBeforeSmoothing=testPorts[0].commands.length; document.querySelector("#scanSmoothing").value='+json.dumps(str(response))+'; document.querySelector("#scanSmoothing").dispatchEvent(new Event("change"))')
    until('scanForTest.latest.sweeps >= sweepsBeforeSmoothing + 4')
    assert js('testPorts[0].commands.length === commandsBeforeSmoothing && scanForTest.plots.rows >= scanForTest.latest.sweeps')
    if response == 0:
        assert js('scanForTest.latest.smoothed.every((v,i) => Object.is(v,scanForTest.latest.current[i]))')
for view in ['average', 'peak', 'occupancy', 'current']:
    js('document.querySelector(\'[data-scan-view="' + view + '"]\').click()')
    expected = 'smoothed' if view == 'current' else view
    assert js('scanForTest.plots.levels(scanForTest.latest) === scanForTest.latest.' + expected)
js('document.querySelector("#scanReset").click()')
until('Number(document.querySelector("#scanWaterfall").dataset.sweeps) >= 2')
js('document.querySelector("#scanStop").click()')
until('document.querySelector("#connectionStatus").textContent === "Connected"')
js('document.querySelector("#liveTab").click()')
until('!document.querySelector("#livePanel").hidden')
js('document.querySelector("#disconnectButton").click()')
until('document.querySelector("#connectionStatus").textContent === "Disconnected"')
assert js('testPorts[0].closed && !testPorts[0].readable.locked && !testPorts[0].writable.locked')
# Reconnect, process maximum size, then unplug.
js('document.querySelector("#sampleCount").value="16384"; document.querySelector("#diversityToggle").checked=false; document.querySelector("#connectButton").click()')
until('document.querySelector("#connectionStatus").textContent === "Connected"')
js('document.querySelector("#startButton").click()')
until('testPorts[1].sequence > 12')
# An unplug during calibration also exits the measuring step.
js('document.querySelector("#calibrateButton").click(); document.querySelector("#calibrationStartButton").click()')
js('clearInterval(testPorts[1].timer); testPorts[1].controller.close()')
until('document.querySelector("#connectionStatus").textContent === "Disconnected"')
assert js('document.querySelector("#statusMessage").classList.contains("error") && testPorts[1].closed')
assert js('document.querySelector("#calibrationTitle").textContent === "Measurement stopped"')
js('document.querySelector("#calibrationDoneButton").click()')
# Unplugging during a scan exits calibration and releases the port too.
js('document.querySelector("#wideTab").click(); document.querySelector("#connectButton").click()')
until('document.querySelector("#connectionStatus").textContent === "Connected"')
js('document.querySelector("#scanProfile").value="fast"; document.querySelector("#scanProfile").dispatchEvent(new Event("input")); document.querySelector("#scanStart").click()')
until('Number(document.querySelector("#scanWaterfall").dataset.sweeps) >= 1')
js('document.querySelector("#scanCalibrate").click(); document.querySelector("#scanCalibrationStart").click(); clearInterval(testPorts[2].timer); testPorts[2].controller.close()')
until('document.querySelector("#connectionStatus").textContent === "Disconnected"')
assert js('testPorts[2].closed && document.querySelector("#scanCalibrationTitle").textContent === "Calibration interrupted"')
js('document.querySelector("#scanCalibrationDone").click(); document.querySelector("#scanAdvanced").open=false; document.querySelector("#liveTab").click()')
until('!document.querySelector("#livePanel").hidden')
js('document.querySelector("#advancedSettings").open=false')
for width, height in [(1440,1000), (1366,768), (960,720), (390,844), (320,700)]:
    call('Emulation.setDeviceMetricsOverride', {'width':width,'height':height,'deviceScaleFactor':1,'mobile':False})
    js('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))')
    metrics = js('''({width: innerWidth, height: innerHeight, scrollWidth:document.documentElement.scrollWidth,
      scrollHeight:document.documentElement.scrollHeight, waterfall:document.querySelector('#waterfallCanvas').clientHeight,
      spectrum:document.querySelector('#spectrumCanvas').clientHeight})''')
    assert metrics['scrollWidth'] <= width, metrics
    assert metrics['waterfall'] > 60 and metrics['spectrum'] > 60, metrics
    overflow = js('''[...document.querySelectorAll('main > *, .control-strip, .calibration-bar, .calibration-guide')]
      .filter(el => el.getClientRects().length && el.getBoundingClientRect().right > document.documentElement.clientWidth + 1)
      .map(el => ({tag:el.tagName, class:el.className, right:el.getBoundingClientRect().right, width:document.documentElement.clientWidth}))''')
    assert not overflow, overflow
    print('Layout:', metrics)
    js('document.querySelector("#advancedSettings").open = true')
    for section in ['Receiver', 'Display', 'Calibration']:
        js(f'document.getElementById("live{section}Tab").click()')
        assert js('document.documentElement.scrollWidth <= innerWidth')
        assert js('document.querySelectorAll("#advancedSettings [data-settings-panel]:not([hidden])").length===1')
        if width in [1440, 390]: shot(f'sdr-settings-live-{section.lower()}-{width}')
    assert js('document.documentElement.scrollWidth <= innerWidth')
    assert js('document.querySelector("#waterfallCanvas").clientHeight === 0 && document.querySelector("#spectrumCanvas").clientHeight > 60')
    js('document.querySelector("#advancedSettings").open = false')
    assert js('document.querySelector("#waterfallCanvas").clientHeight > 60 && document.querySelector("#spectrumCanvas").clientHeight > 60')
    js('document.querySelector("#wideTab").click()')
    until('!document.querySelector("#widePanel").hidden')
    for expanded in [False, True]:
        js('document.querySelector("#scanAdvanced").open = ' + str(expanded).lower())
        js('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))')
        assert js('document.querySelector("#scanSpectrum").clientHeight ' + ('=== 0' if expanded else '> 60') + ' && document.querySelector("#scanWaterfall").clientHeight > 60')
        assert js('''[...document.querySelectorAll('#widePanel > *, #widePanel .control-strip, #widePanel .calibration-bar')]
          .every(el => !el.getClientRects().length || el.getBoundingClientRect().right <= document.documentElement.clientWidth + 1)''')
        if expanded:
            for section in ['Receiver', 'Display', 'Calibration']:
                js(f'document.getElementById("scan{section}Tab").click()')
                assert js('document.documentElement.scrollWidth <= innerWidth')
                if width in [1440, 390]: shot(f'sdr-settings-scan-{section.lower()}-{width}')
    js('document.querySelector("#scanAdvanced").open=false')
    if width == 390:
        js('document.documentElement.dataset.theme="light"')
        shot('sdr-wideband-mobile')
        js('document.documentElement.dataset.theme="dark"')
    js('document.querySelector("#liveTab").click()')
    until('!document.querySelector("#livePanel").hidden')
call('Emulation.setDeviceMetricsOverride', {'width':390,'height':844,'deviceScaleFactor':1,'mobile':False})
js('new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)))')
js('document.documentElement.dataset.theme = "light"')
shot('sdr-aligned-mobile')
js('document.querySelector("[data-help-open]").click()')
assert js('!document.querySelector(".tool-help-overlay").hidden')
js('document.dispatchEvent(new KeyboardEvent("keydown", {key:"Escape"}))')
assert js('document.querySelector(".tool-help-overlay").hidden')
# Legacy firmware still streams and clearly distinguishes the crop from an actual RX width.
js('''(async()=>{
 const {FakePort,makeFrame,infoLine}=await import('./tests/protocol-tests.js');
 Object.defineProperty(navigator.serial,'requestPort',{configurable:true,value:async()=>{
  const port=new FakePort({info:infoLine.replace(/ rates_hz=\\S+| gain_control=1| gain_max=82/g,''),onWrite(command,device){
   if(command.startsWith('STREAM ')) { const f=command.split(' '); device.feed(makeFrame({centerHz:Number(f[1])*1e6,count:Number(f[2])})); }
   if(command==='STOP') device.feed(makeFrame({type:2}));
  }}); window.legacyPort=port; return port;
 }});
})()''')
js('document.querySelector("#sampleCount").value="4096"; document.querySelector("#connectButton").click()')
until('document.querySelector("#connectionStatus").textContent === "Connected"')
assert js('document.querySelector("#rxPreset").disabled && document.querySelector("#rxSummary").textContent.includes("unknown")')
js('document.querySelector("#startButton").click()')
until('document.querySelector("#streamStats").textContent.includes("1 received")')
assert js('legacyPort.commands.some(c=>c.startsWith("STREAM ")) && !legacyPort.commands.some(c=>c.startsWith("BANDWIDTH"))')
js('document.querySelector("#disconnectButton").click()')
until('document.querySelector("#connectionStatus").textContent === "Disconnected"')
assert js('legacyPort.closed')
# Independent NumPy reference: full FFT parity plus overlapping Welch density.
import numpy as np
def iq_samples(payload, n, np):
    words = np.frombuffer(payload, dtype='<u4')
    i = ((words & 1023).astype(np.int32) ^ 512) - 512
    q = (((words >> 10) & 1023).astype(np.int32) ^ 512) - 512
    return (i - 1j*q).astype(np.complex64)
def spectrum_db(samples, window, np):
    magnitude = np.abs(np.fft.fftshift(np.fft.fft(samples * window)))
    return 20 * np.log10(np.maximum(magnitude / (np.sum(window, dtype=np.float64) * 512), 1e-10))
worst = 0
for n in (256,512,1024,2048,4096,8192,16384):
    actual = js('''(async () => {
      const {Spectrum} = await import('./src/Spectrum.js');
      const {Pipeline} = await import('./src/Pipeline.js');
      const {validateConfig} = await import('./src/SdrProtocol.js');
      const n=%d, payload=new Uint8Array(n*4), view=new DataView(payload.buffer);
      for(let i=0;i<n;i++) view.setUint32(i*4,((i*37)%%1024)|(((i*91)%%1024)<<10),true);
      const config=validateConfig(2397,2477,n), pipe=new Pipeline(config,{mode:'welch'});
      const spec=new Spectrum(config);
      return {raw:Array.from(spec.compute(payload)),dc:Array.from(spec.compute(payload,true)),welch:Array.from(pipe.welchPower(payload))};
    })()''' % n)
    indices = np.arange(n,dtype=np.uint32)
    payload = (((indices*37)%1024) | (((indices*91)%1024)<<10)).astype('<u4').tobytes()
    samples = iq_samples(payload,n,np)
    for field, values in [('raw',samples),('dc',samples-samples.mean())]:
        expected = spectrum_db(values,np.hanning(n).astype(np.float32),np)
        delta = float(np.max(np.abs(np.array(actual[field])-expected)))
        worst = max(worst,delta)
        assert delta < .005, (n,field,delta)
    length = min(n,max(1024,n//4))
    window = np.hanning(length).astype(np.float32)
    blocks = []
    for start in range(0,n-length+1,length//2):
        segment = samples[start:start+length]
        blocks.append(np.abs(np.fft.fftshift(np.fft.fft((segment-segment.mean())*window)))**2)
    expected = np.mean(blocks,axis=0) / (80e6*np.sum(window.astype(np.float64)**2)*512**2)
    assert np.allclose(actual['welch'],expected,rtol=1e-5,atol=1e-20), n
print('NumPy parity across all seven sizes: max FFT error',worst,'dB; Welch PSD PASS')
assert not errors, errors
print('Live and Wideband lifecycle, all profiles, scan calibration, click-to-Live, unplug, responsive layout, help: PASS; no uncaught errors')
ws.close()
