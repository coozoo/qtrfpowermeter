# ConceptRF RPM-series RF Power Meter — Serial Protocol

Authoritative reference for the binary serial protocol spoken by the
ConceptRF "RPM" family of USB RF power meters (RPM-20GS, RPM-3GS,
RPM-9G, RPM-6GH and their internal variants). All facts in this
document are extracted from the manufacturer's decompiled Windows
application; file:line references point into `rfpm/rfpm/` in this repo.

## 1. Transport

| Property | Value |
|---|---|
| Layer | USB CDC virtual COM port (CH340 / WCH HL340 USB-to-UART class is observed) |
| Baud rate | 460 800 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| Topology | Single host ↔ single device |

Source: `RF_power_meter.cs:1073` (`serialPort.BaudRate = 460800`).

## 2. Framing

Every message in either direction has the same shape:

```
+------+------+------+------+------+----------+----------+
| 0x55 | 0xAA | cmd  | len  | ~len | data[0]..| checksum |
|      |      |      |      |      | data[L-1]|          |
+------+------+------+------+------+----------+----------+
   0     1     2     3     4     5 .. 5+L-1     5+L
```

- **Sync bytes** — `0x55 0xAA` mark the start of every frame.
- **`cmd`** — one byte. Same field for both request and response. Acts
  as the command code on host→device frames and the response code on
  device→host frames. There is no separate "type" field.
- **`len`** — one byte; the length of the `data` payload (0..255).
- **`~len`** — bitwise complement of `len`. Used as a header-sanity check
  by the receiver; `(len ^ ~len) == 0xFF` must hold.
- **`data`** — `len` payload bytes (may be empty).
- **`checksum`** — one byte. Unsigned 8-bit sum of the bytes
  `cmd + len + ~len + data[0] + … + data[len-1]`, taken modulo 256.
  The sync bytes are **not** included.

Total wire size = `6 + len`.

Receiver behaviour (`RF_power_meter.cs:322-336`):

1. Accumulate incoming bytes into a buffer.
2. Wait until `>= 6` bytes are present.
3. Validate `buf[0]==0x55 && buf[1]==0xAA && buf[3]==(byte)~buf[4]`.
4. If `len + 6` bytes are not yet present, return and wait for more.
5. Sum bytes `[2..len+4]` and compare to `buf[len+5]`.
6. On checksum match, dispatch on `buf[2]` (see §5).
7. On any structural failure, scan forward from index 1 for the next
   `0x55` and resync (`RF_power_meter.cs:388`).

Transmitter (`RF_power_meter.cs:396-411`) builds the frame byte-for-byte
as above and writes it in a single `serialPort.Write` call.

## 3. Encoding & byte order

| Field type | Order |
|---|---|
| Multi-byte integer (`u16`, `u32`, `s32`) in framing data | **Big-endian** (MSB first) |
| IEEE-754 `float32` in calibration payloads | **Little-endian** (host-native, written via `BitConverter.GetBytes` on x86/x64) |

The raw ADC sample on response `0x06` is a signed integer encoded into a
32-bit field; in practice it is a signed **24-bit** value sign-extended
into 32 bits (the per-model `voltage_32_to_voltage` formula divides by
`8388607 = 2^23 - 1`).

## 4. Host → device commands

All four observed commands. `len` is fixed per command.

### `0x80` — Identify

- `len` = `0`
- payload = empty
- Sent by the host on every initialisation tick until a `0x00` response
  arrives (`RF_power_meter.cs:710`).
- Triggers response `0x00`.

### `0x83` — Set sampling rate

- `len` = `1`
- payload = `[sampling_index]` — one byte. Selects an entry in the
  per-model sampling-rate combobox (e.g. AD8362 and ARW family devices
  populate the combobox from the host side based on `device_id`).
- Sent during init with `0` (default), and again when the user changes
  the sampling control (`RF_power_meter.cs:714, 903`).
- Triggers response `0x03`.

### `0x85` — Write calibration entry

- `len` = `POWER_TABLE_NUM_MAX * 4 + 2`
  (e.g. for ARW28340 → `21 * 4 + 2 = 86`)
- payload layout:
  - bytes `[0..1]` — `u16 freq_index` (big-endian); 0..`FREQ_TABLE_NUM_MAX-1`
  - bytes `[2..5+POWER_TABLE_NUM_MAX*4-1]` — `N` × `float32`
    little-endian, one per power point at this frequency
- Used by the host's calibration UI to overwrite the per-frequency
  detector-voltage curve. No response is expected.
- Reference: `calibration_update`, `RF_power_meter.cs:761-779`.

### `0x86` — Read calibration entry

- `len` = `2`
- payload = `[u16 freq_index]` (big-endian)
- Special value `freq_index == 0xFFFF` is the "read all" handshake used
  during initialisation (`RF_power_meter.cs:722-725`); the device then
  streams one `0x05` response per frequency point.
- For a regular numeric `freq_index`, the device replies with a single
  `0x05` for that point.
- Triggers response `0x05`.

## 5. Device → host responses

Dispatch on `rx_data_buff[2]` (`RF_power_meter.cs:338`). All numeric
fields shown below are within the response's `data` payload (starting at
offset `5` in the raw frame).

### `0x00` — Identify

- payload length = 8 bytes
- layout (all big-endian):
  - `data[0..1]` — `u16 device_id`
  - `data[2..3]` — `u16 firmware_version` (display: `version / 100.0` e.g. `0x0069 → 1.05`)
  - `data[4..7]` — `u32 serial_number`
- Reference: `RF_power_meter.cs:341-343`.
- **The model name is NOT returned**. The host maps `device_id` to a
  printable model string in `RF_power_meter.cs:531-614`. See §6.

### `0x03` — Sampling rate ack

- payload length = 1 byte
- layout: `data[0]` — `u8 sampling_index` (echoes the value the device
  actually applied)
- Reference: `RF_power_meter.cs:346-348`.

### `0x05` — Calibration entry

- payload length = `POWER_TABLE_NUM_MAX * 4 + 2` bytes
- layout:
  - `data[0..1]` — `u16 freq_index` (big-endian)
  - `data[2..2+POWER_TABLE_NUM_MAX*4-1]` — `N` × `float32`
    little-endian, the detector voltages at each calibration power point
    for this frequency
- The host stores the floats into `table_voltage[freq_index][j]` and
  marks `table_voltage_init_flag[freq_index] = true` so the next
  init-state tick knows to ask for the next missing entry.
- Reference: `RF_power_meter.cs:350-373`.

### `0x06` — Streaming power sample

- payload length = 4 bytes
- layout: `data[0..3]` — `s32 power_raw` (big-endian); semantically a
  signed 24-bit ADC reading sign-extended to 32 bits
- The host appends each raw value to a FIFO and once enough samples are
  accumulated:
  - converts each `power_raw` to millivolts via the per-model
    `voltage_32_to_voltage`: `power_raw * 1500.0 / 8388607.0` (mV).
    All seven models use this same formula.
  - converts mV at the current `freq_index` to dBm via the per-model
    `adc_convert_power(freq, voltage)` — bilinear interpolation over
    the calibration grid; see §7.
- Reference: `RF_power_meter.cs:375-381` and `:450-453`.

## 6. Device-ID → model/sensor map

| `device_id` | Model name (host UI) | Sensor chip | `FREQ_TABLE_NUM_MAX` | `POWER_TABLE_NUM_MAX` | Freq range | Power range |
|:-:|---|---|:-:|:-:|---|---|
| 101 | RPM-20GS | ARW28340 | 202 | 21 | 10 MHz – 20 GHz | −40 … +10 dBm |
| 102 | RPM-3GS  | AD8362   |  33 | 25 | 50 Hz – 3 GHz   | −50 … +10 dBm |
| 103 | RPM-9G   | ARW22347 |  92 | 21 | 10 MHz – 9 GHz  | −40 … +10 dBm |
| 104 | RPM-20GS (variant) | ARW28340 | 218 | 21 | 10 MHz – 20 GHz | −40 … +10 dBm |
| 105 | (RPM-3GS variant) | AD8362 | 58 | 25 | 50 Hz – 3 GHz | −50 … +10 dBm |
| 106 | (RPM-9G variant)  | ARW22347 | 99 | 21 | 10 MHz – 9 GHz | −40 … +10 dBm |
| 107 | RPM-6GH  | ARW22283 | 69 | 41 | 10 MHz – 6 GHz | −80 … +20 dBm |

The "Model name (host UI)" column is what the Windows app displays in
its label. The device sends only the numeric `device_id`. Power-point
spacing is `(POWER_MAX − POWER_MIN) / (POWER_TABLE_NUM_MAX − 1)` — e.g.
2.5 dB for the 21-point grids, 2.5 dB for the 25-point grids, 2.5 dB for
the 41-point grid.

References: `lookup_table_class.cs:43-145`, individual `lookup_table_*.cs`.

## 7. Voltage → power decode

For every model the raw 32-bit ADC reading maps to a detector voltage
(`mV`) by the same formula:

```
voltage_mV = voltage_32 * 1500.0 / 8388607.0
```

(24-bit signed ADC, full-scale = 1500 mV.)

Then `adc_convert_power(freq, voltage_mV)` interpolates dBm bilinearly
over the calibration grid (per-model file, e.g.
`lookup_table_101_ARW28340.cs:83-144`):

1. Find the two frequency knots `f1, f2` in `table_freq[]` such that
   `f1 ≤ freq ≤ f2`. If `freq` is outside the table, return `100000`
   (sentinel = "over-range").
2. For each of `f1` and `f2`:
   - If `voltage ≥ table_voltage[knot][POWER_TABLE_NUM_MAX-1]` → return
     `100000` (sentinel; signal is below the detector's noise floor for
     this curve, i.e. effectively at or above the highest calibrated
     voltage, depending on sensor polarity).
   - If `voltage < table_voltage[knot][0]` → use the closed-form
     extrapolation `power = POWER_MIN + voltage * (POWER_MIN - 3000)`
     used at the bottom edge of the curve.
   - Otherwise, walk the per-knot voltage column to find the two power
     points `[j, j+1]` straddling `voltage`, then linear-interpolate
     `power[j] + (voltage - V[j]) / (V[j+1] - V[j]) * (power[j+1] - power[j])`.
3. Combine the two knot-power results by linear weighting on
   frequency: `proportion = (freq - f1) / (f2 - f1)` (clamped to 1.0),
   `power = power1 * (1 - proportion) + power2 * proportion`.
4. Add the user-configured `offset` in dB
   (`RF_power_meter.cs:457`).
5. If the final value is `100000` the host displays "Out of range!".

## 8. Initialisation sequence

The host runs a small state machine driven by a periodic timer
(`timer_tx_Tick`, `RF_power_meter.cs:698`) plus per-response flags.

```
                       ┌────────────────────────────────────────┐
                       │                                        │
   state = 0           ▼                                        │
   ─────────► TX 0x80 (Identify)                                │
                       │                                        │
            RX 0x00 ───┼─► id_update_flag = 1                   │
                       │       │                                │
                       │       ▼                                │
                       │   host configures lookup_table         │
                       │   from device_id, then sets            │
                       │   state := 1                           │
                       │                                        │
   state = 1           ▼                                        │
   ─────────► TX 0x83 (Set sampling=0)                          │
                       │                                        │
            RX 0x03 ───┼─► config_update_flag = 1               │
                       │       │                                │
                       │       ▼                                │
                       │   if state==1: state := 2              │
                       │                                        │
   state = 2           ▼                                        │
   ─────────► TX 0x86 (Read all, freq_index=0xFFFF)             │
              clear table_voltage_init_flag[]                   │
              state := 3                                        │
                       │                                        │
   state = 3           ▼                                        │
   loop:    RX 0x05 ───► fill one calibration row,              │
            ─────────►   mark its flag, set cal_flag=1          │
                                                                │
            on next tick with cal_flag==1: cal_flag := 2        │
            on next tick with cal_flag==2:                      │
                find first unfilled freq_index N:               │
                  if N >= 0: TX 0x86 (N)         ────────────── │
                  if N < 0:  state := 10  (streaming, exit init)│
                                                                │
   state = 10          ▼                                        │
   ────────► device autonomously streams 0x06 (power samples)   │
            host averages, applies offset, displays.            │
```

Reference: `RF_power_meter.cs:212` (`initialize_state = 0` on COM open),
`:657, :693` (handshake-driven advances to states 1 and 2), `:708-757`
(per-tick TX dispatch), `:740` (exit to state 10).

## 9. Worked decode example

A device-to-host streaming sample frame:

```
55 AA 06 04 FB 00 24 8E 12 BE
└── sync ──┘ │  │  │  └─── data ──┘ └ checksum
             │  │  │
             │  │  └ ~len = 0xFB    (NOT(0x04) = 0xFB) ✓
             │  └ len = 0x04
             └ cmd/response = 0x06 (streaming power sample)
```

Checksum verification: `0x06 + 0x04 + 0xFB + 0x00 + 0x24 + 0x8E + 0x12 = 0x1BE`,
low byte = `0xBE`. ✓

Decode:

1. `power_raw` = `0x00248E12` (big-endian) = `2 395 666` (decimal,
   positive 24-bit).
2. `voltage_mV = 2 395 666 * 1500 / 8388607 ≈ 428.3 mV`.
3. Assume the current host-selected `freq_index` corresponds to e.g.
   2.4 GHz on a model 101 (RPM-20GS). Look up `table_voltage[freq_2400MHz][]`
   and the bracketing knots in `table_voltage[freq_2300MHz][]` and
   `table_voltage[freq_2500MHz][]`, perform the bilinear interpolation
   from §7, add the user offset, and emit the resulting dBm value.

(Concrete dBm depends on the device's calibration table, which is
device-specific and read from the unit at startup.)

## 10. Quick reference

### Host → device

| `cmd` | Name | `len` | Payload | Notes |
|:-:|---|:-:|---|---|
| `0x80` | Identify | 0 | — | Init only |
| `0x83` | Set sampling | 1 | `u8 idx` | Init + on UI change |
| `0x85` | Write calibration | `4N+2` | `u16 freq_idx BE` + `N×float32 LE` | Calibration UI |
| `0x86` | Read calibration | 2 | `u16 freq_idx BE`; `0xFFFF` = all | Init + manual refresh |

### Device → host

| `cmd` | Name | `len` | Payload |
|:-:|---|:-:|---|
| `0x00` | Identify | 8 | `u16 device_id BE`, `u16 fw_ver BE`, `u32 sn BE` |
| `0x03` | Sampling ack | 1 | `u8 applied_idx` |
| `0x05` | Calibration row | `4N+2` | `u16 freq_idx BE` + `N×float32 LE` |
| `0x06` | Power sample | 4 | `s32 raw BE` (24-bit signed in 32) |

`N = POWER_TABLE_NUM_MAX` (model-dependent; see §6).

## 11. Implementation notes

- The protocol has **no error response, no NACK, no sequence number**.
  Retries are timer-driven by the host (it keeps sending the relevant
  request until the expected response arrives).
- The framing is robust to single-byte corruption: the receiver resyncs
  by scanning forward for the next `0x55`. There is no escaping —
  payload bytes equal to `0x55` are fine because the receiver always
  validates `0xAA` immediately after and falls through to resync if not.
- All multi-byte integers on the wire are **big-endian** but the
  `float32` payload words are **little-endian**. This is not a contradiction
  in the spec — it is what the firmware emits and what `BitConverter`
  on the host parses.
- The init handshake is gated on response arrival, not on time. A host
  implementation should drive it from response handlers rather than from
  a periodic poll on its own.
