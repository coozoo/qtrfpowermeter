/*
 * quicktinysa
 *
 * Copyright (c) 2025 coozoo
 *
 * SPDX-License-Identifier: MIT
 *
 * Created: 2025-10-28 06:53:26
 */

#ifndef TINYSADEVICE_H
#define TINYSADEVICE_H

#include "serialportinterface.h"
#include <QTimer>
#include <QQueue>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include <functional>

// CommandTransaction is the unit of TinySA wire interaction. Each one represents
// "send this string, then this is how the response looks, then call this parser
// on the chunk". The dispatcher honours one-in-flight-at-a-time, which is what
// the protocol actually supports: the device acks completion only via the `ch> `
// prompt, never per-command.
struct CommandTransaction {
    enum class Kind {
        // Text response, ends with `ch> `. payload is the lines between the
        // command echo and the prompt (echo and prompt stripped). The parser is
        // optional; FireAndForget commands set it to nullptr.
        Text,
        // Binary block of exactly `expectedBytes`, then `ch> `. parser receives
        // the raw bytes (no prompt).
        BinaryFixed,
        // Continuous binary stream of byte-counted frames. parser is the inner
        // scan framer; the transaction never auto-completes. Cancelled by an
        // `abort` transaction.
        BinaryContinuous,
    };

    QByteArray  wire;             // command line, no terminator. e.g. "version"
    Kind        kind = Kind::Text;
    int         timeoutMs = 1500; // per-transaction watchdog. -1 disables.
    int         expectedBytes = 0;// BinaryFixed only

    // Callbacks. Exactly one is used per Kind.
    std::function<void(const QByteArrayList &lines)>   onText;
    std::function<void(const QByteArray &block)>       onBinaryFixed;
    // Returns true when the framer wants to end the transaction. Otherwise the
    // continuous stream keeps running and the dispatcher waits for an abort.
    std::function<bool(QByteArray &streamBuffer)>      onBinaryContinuous;

    // Optional handler called when the device responds with a parser-error
    // line starting with `?` while this transaction is the head. Default is to
    // log and complete the transaction.
    std::function<void(const QByteArray &errorLine)>   onReject;

    // Debug label that gets logged; defaults to wire.
    QByteArray  label;
};

// Level envelope per generator mode. The firmware advertises mode-only
// ranges in its `level` usage string but actually routes the output through
// different signal paths depending on frequency, so the *real* achievable
// level shrinks above the path boundaries below.
//
// Source: tinySA/sa_core.c:208 set_output_path() picks the signal path from
// the frequency. tinySA/nanovna.h pins the boundaries. The drive levels for
// each path are in si_drive_dBm[] and adf_drive_dBm[] (sa_core.c:127-130).
struct TinySaLevelEnvelope {
    double minDbm;
    double maxDbm;
};
namespace TinySaLevel {
    // Coarse mode-only envelopes (what the firmware advertises via `level`).
    inline constexpr TinySaLevelEnvelope LowOutput  { -76.0,  -6.0 };
    inline constexpr TinySaLevelEnvelope HighOutput { -38.0, +13.0 };
}

// Real firmware constants. These are what level_min()/level_max() use
// internally before adding the per-unit calibration offset. The `usage`
// strings advertised by `cmd_level` ignore the offset entirely, which is
// why probing `level` gives a useless "-76..-6" answer on every unit.
//
//   actual_max = SL_GENLOW_LEVEL_MAX + low_level_output_offset
//   actual_min = SL_GENLOW_LEVEL_MIN + low_level_output_offset
//
// e.g. an Ultra with `low_level_output_offset = -5` can produce
//   max = -18.5 + (-5) = -23.5 dBm    (matches what was observed on our test unit)
//   min = -115  + (-5) = -120  dBm
namespace TinySaFirmware {
    inline constexpr double GenLowLevelMinUltra = -115.0;  // sa_core.c:155
    inline constexpr double GenLowLevelMaxUltra = -18.5;   // sa_core.c:156
    inline constexpr double GenLowLevelMinBasic = -76.0;   // sa_core.c:477
    inline constexpr double GenLowLevelMaxBasic = -7.0;    // sa_core.c:478
    inline constexpr double GenHighLevelMin     = -38.0;   // sa_core.c:475
    inline constexpr double GenHighLevelMax     = +9.0;    // sa_core.c:476
}

// Per-unit calibration offsets read from the device via `leveloffset` (no
// args). Static after the read; re-probe if the user runs `leveloffset`
// from the command line.
struct TinySaLevelOffsets {
    double lowOutput     = 0.0;  // config.low_level_output_offset
    double highOutput    = 0.0;  // config.high_level_output_offset (Basic only)
    double directOutput  = 0.0;  // config.direct_level_output_offset (Ultra)
    double adfOutput     = 0.0;  // config.adf_level_offset (Ultra, PATH_LEAKAGE)
    bool   valid = false;
};

// Real per-frequency output correction table read from the device via
// `correction out`. This is the table the firmware itself uses inside
// get_frequency_correction() (sa_core.c:2262) to compute the per-freq
// clamp.  Formula the firmware applies inside set_output_path():
//
//   effective_max(f) = SL_GENLOW_LEVEL_MAX + low_output_offset
//                    - interpolate(correction_out, f)
//
// where interpolate is linear between adjacent points, with endpoint
// clamping outside the table range.  No more hardcoded constants.
struct TinySaCorrectionPoint {
    quint64 freqHz;
    double  valueDb;
};

// Frequency-banded envelope for M_GENLOW on TinySA Ultra.  Above 823 MHz
// the firmware silently downgrades through DIRECT / ULTRA / LEAKAGE paths
// and the achievable maximum drops a lot. These rows reflect that. Lower
// bound stays at -76 dBm; the firmware reaches it via the programmable
// attenuator on every path.
//
// The boundaries come from:
//   tinySA/nanovna.h:169 MINIMUM_DIRECT_FREQ  = 823 MHz
//   tinySA/nanovna.h:147 MAX_LO_FREQ          = 959.8 MHz
//   tinySA/nanovna.h:166 MAX_LOW_OUTPUT_FREQ  = 1.13 GHz
//   tinySA/nanovna.h:150 ULTRA_MAX_FREQ       = 1.39 GHz
// The upper bounds above 823 MHz are best-effort approximations from the
// path drive tables; the only ground truth is what the power meter measures.
// Refine these once you have empirical data and we will treat them as
// authoritative.
struct TinySaFreqBandEnvelope {
    quint64 freqMinHz;       // inclusive
    quint64 freqMaxHz;       // exclusive
    double  minDbm;
    double  maxDbm;
    const char *pathName;    // matches firmware's PATH_* signal-path naming
    const char *note;
};

namespace TinySaLevel {
    inline constexpr TinySaFreqBandEnvelope GenLowUltraBands[] = {
        {          0ULL,    823000000ULL, -76.0,  -6.0,  "PATH_LOW",
          "SI446x direct path, well-calibrated factory range" },
        {  823000000ULL,   1130000000ULL, -76.0, -20.0,  "PATH_DIRECT",
          "Direct sampling path; max output drops by ~15 dB vs PATH_LOW" },
        { 1130000000ULL,   1390000000ULL, -76.0, -30.0,  "PATH_ULTRA",
          "Via mixer (only when setting.mixer_output is on)" },
        { 1130000000ULL,   4400000000ULL, -76.0, -40.0,  "PATH_LEAKAGE",
          "Harmonic leakage; lossy, weak output, no power calibration" },
    };
}

// Generator mode the user (or the calibration workflow) thinks the device
// is in. Used to choose between LowOutput and HighOutput envelopes when no
// runtime probe has fired yet. Unknown maps to the wider of the two (Low),
// erring towards "let the firmware reject explicitly if we overshoot".
enum class GeneratorMode {
    Unknown,
    LowOutput,
    HighOutput,
};

class TinySaDevice : public SerialPortInterface
{
    Q_OBJECT

public:
    explicit TinySaDevice(QObject *parent = nullptr);

    enum class DeviceType {
        Unknown,
        TinySABasic,
        TinySAUltra
    };
    Q_ENUM(DeviceType)

    // Static envelope lookup. Use this in the UI / calibration grid to clamp
    // levels without waiting on a probe round-trip. The runtime probeLevelRange
    // remains the authoritative final answer because it includes any
    // calibration-data offsets the device applies.
    static TinySaLevelEnvelope levelEnvelope(GeneratorMode mode);
    // Frequency-banded envelope lookup for Low Output mode on Ultra. Returns
    // the conservative envelope for the given frequency; for High Output or
    // Basic device falls back to the mode-only constant. This is what the UI
    // should use whenever it has a known CW frequency.
    static TinySaLevelEnvelope envelopeForFreq(quint64 freqHz,
                                               GeneratorMode mode,
                                               DeviceType type);
    GeneratorMode currentGeneratorMode() const { return m_generatorMode; }
    // Read-only access to the per-freq output correction table that the
    // firmware uses inside get_frequency_correction(). Populated by
    // probeOutputCorrectionTable() after handshake. Linear interpolation
    // (with endpoint clamping) at any freq via correctionAtFreqDb().
    const QVector<TinySaCorrectionPoint> &outputCorrectionTable() const
        { return m_outputCorrection; }
    double correctionAtFreqDb(quint64 freqHz) const;

    bool isScanning() const { return m_continuousScanActive; }
    DeviceType deviceType() const { return m_deviceType; }
    bool isTinySA4() const { return m_deviceType == DeviceType::TinySAUltra; }
    int firmwareVersion() const { return m_firmwareVersion; }

signals:
    void versionInfoReady(const QString &version);
    void deviceInfoReady(const QString &info);
    void sweepDataReady(const QVector<double> &frequencies, const QVector<double> &levels);
    void scanInterrupted();
    void rawDeviceOutput(const QString &data);
    void batteryVoltageReady(double voltage);
    void markerDataReady(int id, double frequency, double level);
    void traceDataReady(int traceId, const QVector<double> &frequencies, const QVector<double> &levels);
    void frequenciesReady(const QVector<double> &frequencies);
    void screenCaptureReady(const QByteArray &imageData);
    void correctionTableReady(const QString &mode, const QVector<QVector<double>> &table);
    void commandResponse(const QString &command, const QString &response);
    // Surfaced when the device rejects a command with a `?` line or a
    // `usage: ...` hint. The ResponseKind::Text transaction completes after
    // this fires.
    void commandRejected(const QString &command, const QString &reason);
    // Parsed readback of bare `sweep` (no args). The protocol does not
    // provide a per-field query, so this signal is the canonical "what is
    // the device currently set to" reference.  Emitted by getSweepSettings()
    // and by the post-set auto-readback on freq/sweep commands.
    void sweepSettingsReady(double startHz, double stopHz, int points);
    // Parsed readback of bare `level` (no args). The firmware returns
    // `usage: level X..Y` with X and Y being the **currently valid** level
    // range for the active mode and frequency. Emitted by probeLevelRange()
    // and by the auto-probe chain after freq/mode changes.
    void levelRangeReady(double minDb, double maxDb);

public slots:
    // Basic Commands
    void sendCommand(const QString &cmd);   // free-form, handled as Text
    void getVersion();
    void getInfo();
    void getHelp();
    void getBatteryVoltage();
    void resetDevice();
    void clearConfig();

    // Frequency Control
    void setFrequency(double freq_hz);
    void setSweep(double start_hz, double stop_hz);
    void setSweepStart(double freq_hz);
    void setSweepStop(double freq_hz);
    void setSweepCenter(double freq_hz);
    void setSweepSpan(double span_hz);
    void setSweepCW(double freq_hz);
    void getFrequencies();
    // `sweep` with no args returns the current sweep settings as multi-line
    // text. Useful as the only practical readback for "what frequency / span /
    // CW is actually set" since there's no per-field query in the protocol.
    void getSweepSettings();
    // Sends bare `level`. The firmware advertises a hardcoded `usage:
    // level X..Y` that ignores the per-unit offset, so this answer is wide
    // and slightly wrong on every real unit. Kept as a sanity-check; the
    // authoritative path is probeLevelOffsets() + envelopeForFreq().
    void probeLevelRange();
    // Sends bare `leveloffset`. The firmware dumps every calibration offset
    // including `low_output`, which we plug into the firmware formula
    // `level_max = SL_GENLOW_LEVEL_MAX + low_output_offset` to compute the
    // *real* achievable envelope. Run once at connect; the offsets are
    // unit-specific and don't move without an `leveloffset` write.
    void probeLevelOffsets();
    const TinySaLevelOffsets &calibrationOffsets() const { return m_calOffsets; }
    // Reads `correction out` and stores the per-freq Low-Output correction
    // table. After this lands, envelopeForFreq()'s max bound uses the
    // actual interpolated correction at the requested frequency, exactly
    // mirroring the firmware's get_frequency_correction() output.
    void probeOutputCorrectionTable();
    // Reads device internal temperature via the hidden `k` command on
    // Ultra. Used to add the firmware's `dt * DB_PER_DEGREE_ABOVE/BELOW`
    // term to the correction so the spinbox max matches the device's
    // setting.level clamp to the dB step. Without this we are off by up
    // to ~0.5 dB depending on board temperature.
    void probeDeviceTemperature();
    double deviceTemperatureC() const { return m_deviceTempC; }

    // Measurement Settings
    void setRbw(const QString &rbw);
    void setRbw(int rbw_khz);
    void setAttenuation(const QString &atten);
    void setAttenuation(int att_db);
    void setLna(bool enabled);
    void setSpurReduction(const QString &mode);
    void setSampleRepeat(int count);
    void setIF(double freq_hz);

    // Measurement Modes
    void setMeasurementMode(const QString &mode);
    void pauseSweep();
    void resumeSweep();
    void getStatus();

    // Display Settings
    void setUnit(const QString &unit);
    void setScale(const QString &scale);
    void setRefLevel(const QString &level);
    void setZeroLevel(double level);

    // Marker Control
    void setMarker(int id, const QString &setting);
    void getMarker(int id);
    void getMarkerPeak(int id);

    // Trace Control
    void getTraceData(int traceId);
    void storeTrace();
    void clearTrace();
    void subtractTrace();

    // Trigger Control
    void setTrigger(const QString &mode);
    void setTriggerLevel(double level_dbm);

    // Generator Control
    void setMode(const QString &range, const QString &dir);   // F-21: explicit pair
    void setGeneratorFrequency(double freq_hz);
    void setGeneratorFrequencyRange(const QString &range);
    void setGeneratorOutput(bool on);
    void setGeneratorLevel(double level_dbm);
    void setGeneratorModulation(const QString &type);
    void setGeneratorModulation(const QString &type, int value);
    void setLevelChange(double delta_db);
    void setSweepTime(double seconds);
    void setSweepVoltage(double voltage);

    // Calibration
    void setCalOutput(const QString &freq);
    void setExternalGain(double gain_db);
    void setActualFreq(double freq_hz);
    void setDac(int value);
    void setVbatOffset(int offset);
    void setLevelOffset(const QString &type, const QString &mode, double error);

    // Correction Tables
    void getCorrection(const QString &mode);
    void setCorrection(const QString &mode, int entry, double freq_hz, double level_db);

    // Color Settings
    void getColors();
    void setColor(int id, uint32_t rgb);

    // Device ID
    void getDeviceId();
    void setDeviceId(int id);

    // Scanning
    void startScan(double start_hz, double stop_hz, int points = 290);
    void startScanRaw(double start_hz, double stop_hz, int points);
    void startContinuousScanRaw(double start_hz, double stop_hz, int points);
    void startScanWithOutput(double start_hz, double stop_hz, int points, int outmask);
    void stopScan();
    void abort();
    void setAbort(bool enabled);

    // Screen Capture
    void captureScreen();

    // SD Card (TinySA Ultra only)
    void listSDCard();
    void readSDFile(const QString &filename);

    // Time (TinySA Ultra only)
    void setTime(const QDateTime &datetime);

    // Touch Screen
    void touchScreen(int x, int y);
    void releaseTouch();
    void startTouchCalibration();
    void startTouchTest();

    // Self Test
    void selfTest(int testId = 0);

    // USB Interface Control
    void setRefresh(bool enabled);

    // Advanced Functions
    void example();
    void threads();

private slots:
    void onSerialDataReceived(const QByteArray &data);
    void onPortOpened();
    void onHandshakeTimeout();
    void onTransactionTimeout();

private:
    // Transaction lifecycle ---------------------------------------------------
    void enqueueText(const QByteArray &wire,
                     std::function<void(const QByteArrayList &)> onText = nullptr,
                     int timeoutMs = 1500);
    void enqueueFireAndForget(const QByteArray &wire);
    void pushTransaction(CommandTransaction tx);
    void dispatchNext();
    void completeInFlight();
    int  getFirmwareVersion(const QString &versionString);
    void detectDeviceType(const QString &versionString);
    double calculateFrequency(int index);
    double adcToDbm(quint16 raw) const;

    // Wire-level data handlers
    void handleTextChunk(const QByteArray &data);
    void handleBinaryFixedChunk(const QByteArray &data);
    void handleBinaryContinuousChunk(const QByteArray &data);

    // Scanraw inner framer state (F-12)
    enum class ScanFramerState { AwaitStart, ReadPoints, AwaitTerminator };
    void startScanFramer(int points);
    bool feedScanFramer(QByteArray &buffer);

    // State -----------------------------------------------------------------
    DeviceType m_deviceType = DeviceType::Unknown;
    int        m_firmwareVersion = 0;
    double     m_deviceScale = 128.0; // 174 for Ultra, 128 for Basic
    // Tracks the last generator mode we asked the device to use. Updated by
    // setMode / setGeneratorFrequencyRange; not a readback. Defaults to
    // LowOutput because that is the only mode reachable on Ultra fw 165 and
    // the most common starting state everywhere else; if we leave it
    // Unknown the envelope lookup falls back to a mode-only constant and
    // ignores the per-unit offset entirely.
    GeneratorMode m_generatorMode = GeneratorMode::LowOutput;
    // Last CW frequency the device confirmed via the `sweep` readback (start
    // == stop). Used by probeLevelRange to emit the *freq-banded* envelope
    // intersected with the firmware's mode-only answer, so the probe does
    // not "widen" the UI clamp back to the unsafe mode-only range above
    // 823 MHz where the path drops to PATH_DIRECT/ULTRA/LEAKAGE.
    quint64 m_currentCwHz = 0;
    // Per-unit calibration offsets, populated by probeLevelOffsets().
    TinySaLevelOffsets m_calOffsets;
    // Last known device internal temperature (degrees C) read via `k`.
    // Used in the temp correction term inside correctionAtFreqDb.
    // NaN-ish sentinel: m_deviceTempValid = false until probe lands.
    double m_deviceTempC = 34.0;
    bool   m_deviceTempValid = false;

    // Per-path Low-Output correction tables, mirrored from the four
    // firmware tables the device exposes via `correction <name>`. Path
    // selection is the same as set_output_path() in the firmware
    // (sa_core.c:208) so the spinbox clamp matches what the device will
    // actually clamp setting.level to.
    //   m_outputCorrection       <- `out`         (PATH_LOW, f < 823 MHz)
    //   m_outputCorrectionDirect <- `out_direct`  (PATH_DIRECT, 823 MHz - 1.13 GHz)
    //   m_outputCorrectionUltra  <- `out_ultra`   (PATH_ULTRA, with mixer_output)
    //   m_outputCorrectionAdf    <- `out_adf`     (PATH_LEAKAGE, default above 1.13 GHz)
    QVector<TinySaCorrectionPoint> m_outputCorrection;
    QVector<TinySaCorrectionPoint> m_outputCorrectionDirect;
    QVector<TinySaCorrectionPoint> m_outputCorrectionUltra;
    QVector<TinySaCorrectionPoint> m_outputCorrectionAdf;

    // Transaction queue
    QQueue<CommandTransaction> m_pending;
    CommandTransaction         m_inFlight;        // valid when m_inFlightValid
    bool                       m_inFlightValid = false;
    QByteArray                 m_textBuffer;      // for Text transactions
    QByteArray                 m_binaryBuffer;    // for BinaryFixed / BinaryContinuous
    bool                       m_echoConsumed = false;
    QTimer                     m_watchdog;

    // Handshake
    QTimer m_handshakeTimer;
    int    m_handshakeRetries = 0;
    bool   m_deviceReady = false;

    // Scanraw framer
    ScanFramerState m_scanFramerState = ScanFramerState::AwaitStart;
    int             m_scanPointsRequested = 0;
    int             m_scanPointsCollected = 0;
    QVector<double> m_currentSweepLevels;
    QVector<double> m_currentSweepFreqs;
    double          m_scanStartHz = 0;
    double          m_scanStopHz  = 0;
    bool            m_continuousScanActive = false;

    // Screen capture
    static constexpr int kScreenWidth = 320;
    static constexpr int kScreenHeight = 240;
    static constexpr int kScreenBytes  = kScreenWidth * kScreenHeight * 2;
};

#endif // TINYSADEVICE_H
