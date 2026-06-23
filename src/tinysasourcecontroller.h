#ifndef TINYSASOURCECONTROLLER_H
#define TINYSASOURCECONTROLLER_H

#include <QObject>
#include <QString>

class TinySaDevice;

// Thin calibration-flavoured facade over TinySaDevice. Hides the
// spectrum-analyser-side methods we never use and exposes only what the
// advanced-calibration workflow needs: connect to a port, query the
// achievable level envelope at a freq, set the generator to a single
// (freq, level) cell, pause/resume between cells.
//
// Not registered with PMDeviceFactory and never shown in the main device
// combobox; the calibration panel owns its lifetime.
//
// Connection flow:
//   connectToPort()  ->  TinySaDevice opens the port, runs version /
//                         leveloffset / correction / k probes, then
//                         enters Low Output mode automatically.
//   connected()      ->  emitted when the probes land and the device is
//                         ready for setOutput() calls.
//   setOutput(f, l)  ->  freq + level set, generator output ON.
//   pause()/resume() ->  generator output OFF/ON without touching freq/level.
//   disconnect()     ->  generator OFF, port closed.
class TinySaSourceController : public QObject
{
    Q_OBJECT
public:
    explicit TinySaSourceController(QObject *parent = nullptr);
    ~TinySaSourceController() override;

    // Achievable level envelope at a given freq. Computed from the
    // firmware-source formula in TinySaDevice (per-path correction table
    // + per-unit offsets + temperature + 0.5 dB rounding). Used by
    // isCellAchievable() and by the calibration auto-fill to skip cells
    // the device cannot deliver. After the first connect probes complete
    // the numbers are firmware-accurate for the connected unit.
    struct LevelEnvelope {
        double minDbm = -115.0;
        double maxDbm = -18.5;
    };
    LevelEnvelope envelopeAt(double freqHz) const;

    // True when the cell falls inside envelopeAt(freqHz). Cheap; the
    // auto-fill calls this for every planned cell before sending anything
    // on the wire, so unreachable cells are silently skipped.
    bool isCellAchievable(double freqHz, double levelDbm) const;

    bool isConnected() const { return m_ready; }
    QString portName() const { return m_portName; }

public slots:
    // Open the named port. Asynchronous: connected() fires once the
    // probes complete; deviceError() fires if the handshake fails.
    void connectToPort(const QString &portName);
    // Generator OFF, port closed. Idempotent.
    void disconnectFromPort();
    // Single calibration cell. Sets freq via `sweep center`, sets level,
    // turns generator output ON if not already. Emits outputSet() once
    // the device's sweep-readback confirms the freq landed. Emits
    // outputClamped() if our formula predicts the asked level exceeds
    // the envelope for this freq (so the caller knows the firmware will
    // silently clamp).
    void setOutput(double freqHz, double levelDbm);
    // Generator output OFF, freq/level retained.
    void pauseOutput();
    // Generator output ON at the last freq/level set.
    void resumeOutput();

signals:
    void connected();
    void disconnected();
    void deviceError(const QString &message);
    void outputSet(double freqHz, double levelDbm);
    // Asked level was above envelope.maxDbm for freqHz. Predicted is what
    // the firmware will actually clamp setting.level to.
    void outputClamped(double freqHz, double askedDbm, double predictedDbm);

private slots:
    void onDeviceVersion(const QString &versionString);
    void onDeviceError(const QString &error);
    void onSweepSettingsReady(double startHz, double stopHz, int points);

private:
    TinySaDevice *m_dev = nullptr;
    QString m_portName;
    bool m_ready = false;          // probes complete, ready for output
    bool m_outputOn = false;       // current generator state
    double m_lastAskedFreqHz = 0.0;
    double m_lastAskedDbm = -1000.0;
};

#endif // TINYSASOURCECONTROLLER_H
