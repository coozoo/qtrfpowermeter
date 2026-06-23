#include "tinysasourcecontroller.h"
#include "tinysadevice.h"

#include <QDebug>

TinySaSourceController::TinySaSourceController(QObject *parent)
    : QObject(parent)
    , m_dev(new TinySaDevice(this))
{
    // Bridge the slim signals the calibration workflow cares about.
    connect(m_dev, &TinySaDevice::versionInfoReady,
            this, &TinySaSourceController::onDeviceVersion);
    connect(m_dev, &SerialPortInterface::serialPortErrorSignal,
            this, &TinySaSourceController::onDeviceError);
    connect(m_dev, &TinySaDevice::sweepSettingsReady,
            this, &TinySaSourceController::onSweepSettingsReady);
    connect(m_dev, &SerialPortInterface::portClosed,
            this, [this]() {
                if (m_ready) {
                    m_ready = false;
                    m_outputOn = false;
                    emit disconnected();
                }
            });
}

TinySaSourceController::~TinySaSourceController()
{
    if (m_ready) {
        // Best-effort generator off and port close on shutdown.
        m_dev->setGeneratorOutput(false);
        m_dev->stopPort();
    }
}

// =============================================================================
//  Envelope query
// =============================================================================

TinySaSourceController::LevelEnvelope
TinySaSourceController::envelopeAt(double freqHz) const
{
    // Mode-only constants come from the firmware source; the per-freq /
    // per-unit refinement comes from the live probes inside TinySaDevice.
    // envelopeForFreq() returns the formula constants; subtracting
    // correctionAtFreqDb(f) plus the parsed offset gives the actual
    // achievable max for THIS device at THIS freq.
    const TinySaLevelEnvelope env = TinySaDevice::envelopeForFreq(
        static_cast<quint64>(freqHz),
        m_dev->currentGeneratorMode(),
        m_dev->deviceType());
    const TinySaLevelOffsets &off = m_dev->calibrationOffsets();
    const double corr = m_dev->correctionAtFreqDb(static_cast<quint64>(freqHz));
    LevelEnvelope out;
    out.minDbm = env.minDbm + off.lowOutput;
    out.maxDbm = env.maxDbm + off.lowOutput - corr;
    return out;
}

bool TinySaSourceController::isCellAchievable(double freqHz, double levelDbm) const
{
    const LevelEnvelope env = envelopeAt(freqHz);
    return levelDbm >= env.minDbm && levelDbm <= env.maxDbm;
}

// =============================================================================
//  Lifecycle
// =============================================================================

void TinySaSourceController::connectToPort(const QString &portName)
{
    if (m_ready && portName == m_portName) return;
    if (m_dev->isPortOpen()) m_dev->stopPort();
    m_portName = portName;
    m_ready = false;
    m_outputOn = false;
    m_dev->setportName(portName);
    // TinySa is USB CDC ACM, baud is ignored but use the documented default.
    m_dev->setbaudRate(115200);
    m_dev->startPort();
    // connected() fires from onDeviceVersion once handshake + probes land.
}

void TinySaSourceController::disconnectFromPort()
{
    if (!m_ready && !m_dev->isPortOpen()) return;
    if (m_ready) m_dev->setGeneratorOutput(false);
    m_dev->stopPort();
    m_ready = false;
    m_outputOn = false;
    emit disconnected();
}

void TinySaSourceController::onDeviceVersion(const QString &versionString)
{
    Q_UNUSED(versionString);
    // The version handshake also chains leveloffset, correction tables,
    // temperature and sweep probes. We mark ready immediately because
    // setOutput() will queue its own sweep/k re-probes anyway; the
    // envelope formula gracefully falls back to the firmware constants
    // until the per-unit values land a few hundred ms later.
    if (m_ready) return;
    m_ready = true;
    // Enter Low Output generator mode. Per TinySa Ultra fw 224, the
    // mode command on this hardware only accepts `[low] input|output`;
    // we send `low output`.  After this the generator chain is ready,
    // output is still off until setOutput() is called.
    m_dev->setGeneratorFrequencyRange("low");
    emit connected();
}

void TinySaSourceController::onDeviceError(const QString &error)
{
    emit deviceError(error);
}

void TinySaSourceController::onSweepSettingsReady(double startHz, double stopHz, int points)
{
    Q_UNUSED(stopHz);
    Q_UNUSED(points);
    // The most recent setOutput() pushed a `sweep center` then a `sweep`
    // readback. When the readback confirms start == stop == asked freq,
    // we can declare the cell set. We compare against m_lastAskedFreqHz
    // because the readback can also be triggered by mode changes etc.
    if (!m_ready) return;
    if (m_lastAskedDbm <= -999.0) return; // setOutput not in flight
    if (!qFuzzyCompare(startHz, m_lastAskedFreqHz)) return;
    emit outputSet(m_lastAskedFreqHz, m_lastAskedDbm);
    m_lastAskedDbm = -1000.0; // consumed
}

// =============================================================================
//  Output control
// =============================================================================

void TinySaSourceController::setOutput(double freqHz, double levelDbm)
{
    if (!m_ready) return;

    // Predict whether the firmware will silently clamp the requested
    // level. The formula has been proven to match set_output_path's
    // setting.level clamp to the dB step on fw 224 (PATH_LOW/DIRECT/
    // ULTRA/LEAKAGE per the path tables).
    const LevelEnvelope env = envelopeAt(freqHz);
    if (levelDbm > env.maxDbm) {
        emit outputClamped(freqHz, levelDbm, env.maxDbm);
        levelDbm = env.maxDbm;
    } else if (levelDbm < env.minDbm) {
        emit outputClamped(freqHz, levelDbm, env.minDbm);
        levelDbm = env.minDbm;
    }

    m_lastAskedFreqHz = freqHz;
    m_lastAskedDbm = levelDbm;

    // Freq first (queues sweep readback inside the driver), then level
    // (queues k temperature re-probe), then ensure output ON. The
    // prompt-paced queue serialises these.
    m_dev->setGeneratorFrequency(freqHz);
    m_dev->setGeneratorLevel(levelDbm);
    if (!m_outputOn) {
        m_dev->setGeneratorOutput(true);
        m_outputOn = true;
    }
}

void TinySaSourceController::pauseOutput()
{
    if (!m_ready || !m_outputOn) return;
    m_dev->setGeneratorOutput(false);
    m_outputOn = false;
}

void TinySaSourceController::resumeOutput()
{
    if (!m_ready || m_outputOn) return;
    m_dev->setGeneratorOutput(true);
    m_outputOn = true;
}
