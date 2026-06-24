# CLAUDE.md — qtrfpowermeterRPM20GS

Guidance for a future session working on this codebase. Captures the non-obvious bits — read the code for the obvious ones.

## Repo layout

This directory is the **variant** of `qtrfpowermeter` that adds the ConceptRF RPM family. The baseline (RF8000, RF-PM V5/V7) lives next door at `../qtrfpowermeter`. Common source files are duplicated between the two projects on purpose — when you change a file that exists in both trees, port the change to both.

```
qtrfpowermeter/                 ← baseline (RF8000 / V5 / V7)
qtrfpowermeterRPM20GS/          ← this project (baseline + ConceptRF)
rfpm/rfpm/                      ← decompiled C# reference app for ConceptRF
docs/protocol/rpm-series.md     ← authoritative protocol spec, extracted from rfpm/
```

The decompiled C# is read-only reference. When in doubt about ConceptRF wire behaviour, trust `rfpm/rfpm/RF_power_meter/RF_power_meter.cs` over your intuition — it's what the real device firmware was designed against. The protocol doc at `docs/protocol/rpm-series.md` summarises the parts we care about with file:line references.

## Build & test

- `qmake6 rf8000.pro && make -j` builds the app. Yes, `rf8000.pro` despite the project handling many devices — that's a historical name.
- Tests live under `tests/`. They are a separate `qmake6 tests/tests.pro` build that doesn't touch the main app. Run with `make check` from `tests/`. Each device has a `tests/<device>_parser/` subdirectory; add a new one when you add a new device.
- Tests drive the parser layer directly via `processData(QString)` or by feeding bytes through `SerialPortInterface::processIncomingBytes(QByteArray)` (see "SerialPortInterface gotcha" below). There is **no** QApplication / GUI integration test infrastructure — the harness can't catch ctor-ordering crashes or signal-wiring bugs unless the bug is exposed through a testable seam.

## Device abstraction

- `AbstractPMDevice` is the base. Concrete devices implement `setFrequency`/`setOffset`/`connectDevice`/`disconnectDevice`/`processData` and emit `measurementReady(QDateTime, double dbm, double vpp_raw)` plus the lifecycle signals.
- `PMDeviceFactory::registerDevices()` is the hardcoded registry. To add a device: append a `PMDeviceProperties`, append an `if` branch in `createDevice()`, write the class, register a tests sub-pro.
- `propertiesUpdated(PMDeviceProperties)` is the signal a device emits when it changes its own props at runtime — e.g. ConceptRF replaces its placeholder freq range with the real per-model range after identify. **It overlays selected fields onto the existing props** (name, freq range, power range, baud, hasOffset). Don't replace the whole struct or you lose `supportedVidPids`/`imagePath`/`alternativeNames`.
- `deviceIdentityChanged(model, fw, sn)` is emitted only by devices that report runtime identity (ConceptRF). Wire-up shows it in the status bar.
- `supportedSamplingRatesHz()` / `setSamplingRateIndex()` are optional. Devices that don't expose programmable rates leave the defaults (empty list, no-op setter) and the UI combobox stays hidden.

## ConceptRF binary protocol

State machine: `Idle → Connecting → Identifying → SettingSampling → DownloadingCalibration → Ready`. Frame format and command/response IDs are in `docs/protocol/rpm-series.md`. A few things that bit us during the rewrite:

- Response `0x06` (streaming sample) is **big-endian** in the spec. The original code decoded it little-endian; readings were nonsense. The test `decodeStreamingSample_isBigEndian` pins the byte order.
- The watchdog timer must only stop on the response that advances the **current** state — `0x00` while `Identifying`, `0x03` while `SettingSampling`, `0x05` while `DownloadingCalibration`. The device emits `0x06` continuously from port-open even before the handshake completes; treating those as "the handshake is alive" wedges the state machine if the device is actually incompatible. See `handlePacket` and the `identificationTimeout_streamingSamplesDoNotResetWatchdog` test.
- Sample rate is **always reset to index 0 (10 Hz) at connect** to match the C# reference app. The combobox is disabled when not connected to prevent pre-selecting a rate that won't match the device's actual state on connect.
- `0xFFFF` as the calibration row index in cmd `0x86` is a special "send all rows" request. We seed the download with it, then top up any missing rows via `firstUnfilledRow()`.

## Lookup-table caveat

`conceptrfrpmlookuptables.cpp` currently allocates one size per "family" (e.g. `Rpm20gsLookupTable` is 202 rows for device_id 101). But the firmware ID space pairs each family with a second variant of different size (device_id 104 = 218 rows for the same family). The driver still selects `Rpm20gsLookupTable` for both IDs — the extra rows would just stay unfilled. Fix when someone reports it; needs `device_id` plumbed into `initializeTables()`.

## SerialPortInterface gotcha

`SerialPortInterface::readData()` was once silently broken: a line that *looked* like an emit was actually a local function declaration (`void serialPortNewBinaryData(const QByteArray&);` instead of `emit ...`). Compiler accepted it, no signal fired, ConceptRF identify timed out forever.

The fix factored the emit path into `processIncomingBytes(QByteArray)` so tests can verify the emit happens. **Don't inline emit calls into `readData()` again** — that breaks the regression test `serialPortBinarySignal_reachesDevice`. Add new signals to `processIncomingBytes` instead.

## MainWindow ordering trap

The constructor's order matters around line ~310:
```
setupSettingsMenu();          // creates m_actionShowLogTab
loadSettings();
updateDeviceList();           // may find an already-plugged device →
                              // performSmartSelection() → setCurrentIndex →
                              // createDevice() reads m_actionShowLogTab
```
If `setupSettingsMenu()` runs after `updateDeviceList()`, the app crashes at startup when a device is already on the bus. There's a null-guard in `createDevice` as belt-and-braces but the ordering is the real fix. Don't reshuffle without checking this.

## VID:PID `1a86:7523` collision

RF8000 family and ConceptRF share the CH340 USB ID. `performSmartSelection()`:
1. First check `QSettings` for the user's last manual pick keyed on `vid_pid`. Use it if the deviceId still exists.
2. Otherwise fall back to "first registered device that claims this VID:PID" (combobox-order heuristic).

`m_inProgrammaticDeviceTypeChange` is the flag that prevents the smart-selection's `setCurrentIndex()` from looking like a user pick and being persisted. Don't drop it.

## Device-calibration viewer (ConceptRF only)

`DeviceCalibrationViewerWidget` does the rendering (table + per-row plot + Export CSV). It's embedded directly in the `CalibrationManager` panel. When the active device is ConceptRF and Ready, `MainWindow::onDeviceConnected` calls `m_calibrationManager->setActiveConceptRfDevice(cdev)`. The manager hides its user-calibration body, locks the mode radios to Disabled (greyed out), and shows the factory widget inline. `onDeviceDisconnected` and `createDevice` call `setActiveConceptRfDevice(nullptr)` to release the lock and restore the persisted mode for the new device id.

The widget snapshots `freqAxisHz / powerAxisDb / voltageTableMv` synchronously in `setDevice()`. Safe because nothing writes to the lookup table after Ready. If you ever start mutating it after Ready, switch to a `Q_INVOKABLE` snapshot copied across threads with `Qt::BlockingQueuedConnection`.

## UI conventions established here

- Status-bar permanent widgets: identity label (model/fw/sn), sampling combobox (visible only when device supports it), "Fast view…" button. Add new persistent affordances there before adding to the menu bar.
- **Never use `QMessageBox::exec()` for "saved" confirmations.** Use `notify::showSavedToast(anchor, message, optional_path_to_open)` from `savedtoast.h`. Frameless, fade in/out, click-to-open-folder if you pass a path. The pattern applies to chart save, fast-view save, and any future save action.
- Fast-view chart plots in **sample-domain**: `x[i] = i / sample_rate`, fixed X axis `[0, windowSeconds]`, data slides across a static grid like an oscilloscope. Don't go back to wall-clock-domain plotting — USB jitter squeezes the trace. See `FastViewDialog::renderFrame` and the C# original at `Measure_UserControl.cs:188-190` for the reference behaviour.
- Long-term chart decimates raw samples to `kChartTargetHz = 10 Hz` before feeding the table/chart/CSV. LCDs and max-tracking still see every sample.

## Documented bugs intentionally left

None right now. (Was: `RfpmV7Device`'s function-`static QString buffer` leaking across instances. Fixed by moving it to `m_buffer`; the test `perInstanceBuffer_noCrossInstanceLeak` pins the fix.)
