/*
 * quicktinysa
 *
 * Copyright (c) 2025 coozoo
 *
 * SPDX-License-Identifier: MIT
 *
 * Created: 2025-10-28 06:49:37
 */

#include "tinysadevice.h"
#include <QDebug>
#include <QRegularExpression>
#include <QtEndian>
#include <QDateTime>
#include <cmath>

// =============================================================================
//  Lifecycle
// =============================================================================

TinySaDevice::TinySaDevice(QObject *parent)
    : SerialPortInterface(parent)
{
    setRawMode(true);
    connect(this, &SerialPortInterface::serialPortNewRawData,
            this, &TinySaDevice::onSerialDataReceived);
    connect(this, &SerialPortInterface::portOpened,
            this, &TinySaDevice::onPortOpened);

    m_watchdog.setSingleShot(true);
    connect(&m_watchdog, &QTimer::timeout,
            this, &TinySaDevice::onTransactionTimeout);

    m_handshakeTimer.setSingleShot(true);
    connect(&m_handshakeTimer, &QTimer::timeout,
            this, &TinySaDevice::onHandshakeTimeout);
}

void TinySaDevice::onPortOpened()
{
    qDebug() << "Port opened, starting handshake.";
    m_handshakeRetries = 0;
    m_deviceReady = false;
    clearInputBuffer();
    m_textBuffer.clear();
    m_binaryBuffer.clear();
    m_pending.clear();
    m_inFlightValid = false;
    m_continuousScanActive = false;
    m_generatorMode = GeneratorMode::LowOutput; // sensible default; freq-band formula needs it
    // Hand the UI a sane envelope before the handshake even starts. Low
    // Output is the most permissive supported mode on every TinySA we've
    // seen, so it's the safe default. The post-handshake probeLevelRange
    // call refines whenever the device reports a tighter range.
    {
        const TinySaLevelEnvelope env = levelEnvelope(GeneratorMode::Unknown);
        emit levelRangeReady(env.minDbm, env.maxDbm);
    }
    getVersion();
}

void TinySaDevice::onHandshakeTimeout()
{
    // Reserved; per-transaction watchdog handles handshake retry.
}

// =============================================================================
//  Transaction dispatch (prompt-paced, one in flight at a time)
// =============================================================================

void TinySaDevice::pushTransaction(CommandTransaction tx)
{
    if (tx.label.isEmpty()) tx.label = tx.wire;
    m_pending.enqueue(std::move(tx));
    if (!m_inFlightValid) dispatchNext();
}

void TinySaDevice::dispatchNext()
{
    if (m_inFlightValid) return;
    if (m_pending.isEmpty()) return;
    if (!isPortOpen()) return;

    m_inFlight = m_pending.dequeue();
    m_inFlightValid = true;
    m_textBuffer.clear();
    m_binaryBuffer.clear();
    m_echoConsumed = false;

    QByteArray wireLine = m_inFlight.wire;
    if (!wireLine.endsWith("\r\n")) wireLine.append("\r\n");
    writeData(wireLine);

    if (m_inFlight.timeoutMs > 0) {
        m_watchdog.start(m_inFlight.timeoutMs);
    }
}

void TinySaDevice::completeInFlight()
{
    m_watchdog.stop();
    m_inFlightValid = false;
    m_textBuffer.clear();
    m_binaryBuffer.clear();
    m_echoConsumed = false;
    dispatchNext();
}

void TinySaDevice::onTransactionTimeout()
{
    if (!m_inFlightValid) return;
    qWarning() << "Transaction timeout for" << m_inFlight.label;

    if (m_inFlight.wire == "version" && !m_deviceReady) {
        m_handshakeRetries++;
        if (m_handshakeRetries < 5) {
            qDebug() << "Handshake attempt" << m_handshakeRetries;
            m_textBuffer.clear();
            clearInputBuffer();
            QByteArray wireLine = m_inFlight.wire + "\r\n";
            writeData(wireLine);
            m_watchdog.start(750);
            return;
        }
        emit serialPortErrorSignal("Connection failed: No response from device.");
    } else {
        emit commandRejected(QString::fromLatin1(m_inFlight.label), "timeout");
    }
    completeInFlight();
}

void TinySaDevice::enqueueText(const QByteArray &wire,
                               std::function<void(const QByteArrayList &)> onText,
                               int timeoutMs)
{
    CommandTransaction tx;
    tx.wire = wire;
    tx.kind = CommandTransaction::Kind::Text;
    tx.timeoutMs = timeoutMs;
    tx.onText = std::move(onText);
    pushTransaction(std::move(tx));
}

void TinySaDevice::enqueueFireAndForget(const QByteArray &wire)
{
    enqueueText(wire, nullptr, 1500);
}

// =============================================================================
//  Serial RX dispatch
// =============================================================================

void TinySaDevice::onSerialDataReceived(const QByteArray &data)
{
    if (!m_inFlightValid) {
        if (!data.isEmpty()) {
            // Spontaneous bytes. Auto-refresh (bulk/fill) demux is future work.
            qDebug() << "Discarding spontaneous bytes" << data;
        }
        return;
    }

    switch (m_inFlight.kind) {
    case CommandTransaction::Kind::Text:
        handleTextChunk(data);
        break;
    case CommandTransaction::Kind::BinaryFixed:
        handleBinaryFixedChunk(data);
        break;
    case CommandTransaction::Kind::BinaryContinuous:
        handleBinaryContinuousChunk(data);
        break;
    }
}

void TinySaDevice::handleTextChunk(const QByteArray &data)
{
    m_textBuffer.append(data);
    if (!m_textBuffer.contains("ch> ")) return;

    int promptIndex = m_textBuffer.indexOf("ch> ");
    QByteArray chunk = m_textBuffer.left(promptIndex);
    m_textBuffer.remove(0, promptIndex + 4);

    const QByteArray echoMatch = m_inFlight.wire;
    QByteArrayList linesRaw = chunk.split('\n');
    QByteArrayList lines;
    QByteArray firstReject;
    bool echoSeen = false;
    for (const QByteArray &raw : linesRaw) {
        QByteArray line = raw;
        if (line.endsWith('\r')) line.chop(1);
        if (line.isEmpty()) continue;
        if (!echoSeen && line == echoMatch) { echoSeen = true; continue; }
        // Firmware "you sent nonsense" markers. The protocol gives no clean
        // ack, so we have to recognise these patterns by hand:
        //   `?` line     - classic parser error (rev5).
        //   `usage: ...` - hint emitted when the command parsed but argument
        //                  shape was rejected (e.g. `mode high output` on a
        //                  Basic whose firmware only knows `mode [low] in|out`).
        if (line.startsWith('?') || line.startsWith("usage:")) {
            if (firstReject.isEmpty()) firstReject = line;
            continue;
        }
        lines.append(line);
    }

    for (const QByteArray &line : lines) {
        emit rawDeviceOutput(QString::fromLatin1(line));
    }

    if (!firstReject.isEmpty()) {
        if (m_inFlight.onReject) {
            m_inFlight.onReject(firstReject);
        } else {
            emit commandRejected(QString::fromLatin1(m_inFlight.label),
                                 QString::fromLatin1(firstReject));
        }
        completeInFlight();
        return;
    }

    if (m_inFlight.onText) {
        m_inFlight.onText(lines);
    } else {
        emit commandResponse(QString::fromLatin1(m_inFlight.label),
                             QString::fromLatin1(chunk).trimmed());
    }
    completeInFlight();
}

void TinySaDevice::handleBinaryFixedChunk(const QByteArray &data)
{
    m_binaryBuffer.append(data);

    if (!m_echoConsumed) {
        QByteArray echoTerm = m_inFlight.wire + "\r\n";
        int idx = m_binaryBuffer.indexOf(echoTerm);
        if (idx < 0) return;
        m_binaryBuffer.remove(0, idx + echoTerm.size());
        m_echoConsumed = true;
    }

    if (m_binaryBuffer.size() < m_inFlight.expectedBytes) return;

    QByteArray block = m_binaryBuffer.left(m_inFlight.expectedBytes);
    m_binaryBuffer.remove(0, m_inFlight.expectedBytes);

    if (m_inFlight.onBinaryFixed) m_inFlight.onBinaryFixed(block);

    int promptIdx = m_binaryBuffer.indexOf("ch> ");
    if (promptIdx >= 0) m_binaryBuffer.remove(0, promptIdx + 4);
    completeInFlight();
}

void TinySaDevice::handleBinaryContinuousChunk(const QByteArray &data)
{
    m_binaryBuffer.append(data);
    if (!m_echoConsumed) {
        QByteArray echoTerm = m_inFlight.wire + "\r\n";
        int idx = m_binaryBuffer.indexOf(echoTerm);
        if (idx < 0) return;
        m_binaryBuffer.remove(0, idx + echoTerm.size());
        m_echoConsumed = true;
    }
    if (m_inFlight.onBinaryContinuous) {
        bool wantsEnd = m_inFlight.onBinaryContinuous(m_binaryBuffer);
        if (wantsEnd) completeInFlight();
    }
}

// =============================================================================
//  Scanraw byte-counted framer (F-12)
// =============================================================================

void TinySaDevice::startScanFramer(int points)
{
    m_scanFramerState = ScanFramerState::AwaitStart;
    m_scanPointsRequested = points;
    m_scanPointsCollected = 0;
    m_currentSweepLevels.clear();
    m_currentSweepFreqs.clear();
}

bool TinySaDevice::feedScanFramer(QByteArray &buf)
{
    // Inner state machine. Returns true if it consumed enough bytes to make
    // progress; the caller invokes us repeatedly until we return false (need
    // more bytes) or we transition to AwaitStart with a completed sweep.
    while (true) {
        if (m_scanFramerState == ScanFramerState::AwaitStart) {
            int idx = buf.indexOf('{');
            if (idx < 0) { buf.clear(); return false; }
            buf.remove(0, idx + 1);
            m_scanFramerState = ScanFramerState::ReadPoints;
            continue;
        }
        if (m_scanFramerState == ScanFramerState::ReadPoints) {
            const int pointSize = 3;
            int remaining = (m_scanPointsRequested - m_scanPointsCollected) * pointSize;
            int available = qMin(buf.size(), remaining);
            int wholePoints = available / pointSize;
            for (int i = 0; i < wholePoints; ++i) {
                const uchar *p = reinterpret_cast<const uchar *>(buf.constData() + i * pointSize + 1);
                quint16 raw = qFromLittleEndian<quint16>(p);
                m_currentSweepLevels.append(adcToDbm(raw));
                m_currentSweepFreqs.append(calculateFrequency(m_scanPointsCollected + i));
            }
            int consumed = wholePoints * pointSize;
            buf.remove(0, consumed);
            m_scanPointsCollected += wholePoints;
            if (m_scanPointsCollected < m_scanPointsRequested) return false;
            m_scanFramerState = ScanFramerState::AwaitTerminator;
            continue;
        }
        if (m_scanFramerState == ScanFramerState::AwaitTerminator) {
            if (buf.size() < 1) return false;
            char first = buf.at(0);
            if (first != '}') {
                // Drift: drop a byte and resync. Indicates a missed sync.
                qDebug() << "Scan framer drift, expected }, got" << QChar(first);
                buf.remove(0, 1);
                m_scanFramerState = ScanFramerState::AwaitStart;
                continue;
            }
            // Need next byte to decide continuous vs end/interrupt.
            if (buf.size() < 2) return false;
            char second = buf.at(1);
            // Emit completed sweep regardless of how it ends.
            emit sweepDataReady(m_currentSweepFreqs, m_currentSweepLevels);
            m_currentSweepLevels.clear();
            m_currentSweepFreqs.clear();
            m_scanPointsCollected = 0;
            if (second == '{') {
                // `}{` = end-of-sweep marker in continuous mode.
                buf.remove(0, 2);
                m_scanFramerState = ScanFramerState::ReadPoints;
                continue;
            }
            // Lone `}` (followed by anything else) = end of one-shot scan
            // or user interrupt (jog / touch). The next bytes are the `ch> `
            // prompt that the dispatcher will drain.
            buf.remove(0, 1);
            m_scanFramerState = ScanFramerState::AwaitStart;
            return true; // caller decides if transaction ends
        }
    }
}

TinySaLevelEnvelope TinySaDevice::levelEnvelope(GeneratorMode mode)
{
    switch (mode) {
    case GeneratorMode::HighOutput: return TinySaLevel::HighOutput;
    case GeneratorMode::LowOutput:  return TinySaLevel::LowOutput;
    case GeneratorMode::Unknown:
    default:                        return TinySaLevel::LowOutput;
    }
}

static double interpolateCorrectionTable(
    const QVector<TinySaCorrectionPoint> &t, quint64 freqHz)
{
    if (t.isEmpty()) return 0.0;
    if (freqHz <= t.first().freqHz) return t.first().valueDb;
    if (freqHz >= t.last().freqHz)  return t.last().valueDb;
    for (int i = 1; i < t.size(); ++i) {
        const auto &lo = t[i - 1];
        const auto &hi = t[i];
        if (freqHz >= lo.freqHz && freqHz <= hi.freqHz) {
            const double span = double(hi.freqHz - lo.freqHz);
            if (span <= 0.0) return lo.valueDb;
            const double s = double(freqHz - lo.freqHz) / span;
            return lo.valueDb + s * (hi.valueDb - lo.valueDb);
        }
    }
    return 0.0;
}

void TinySaDevice::probeDeviceTemperature()
{
    // Hidden `k` command on Ultra prints `<temp>\r\n`. No `ch> ` framing
    // worries; the text dispatch handles the prompt-terminated single line.
    enqueueText("k", [this](const QByteArrayList &lines) {
        for (const QByteArray &l : lines) {
            QString s = QString::fromLatin1(l).trimmed();
            bool ok;
            double v = s.toDouble(&ok);
            if (ok && v > -50.0 && v < 150.0) {
                m_deviceTempC = v;
                m_deviceTempValid = true;
                emit commandResponse("k",
                    QString("device temperature = %1 deg C").arg(v, 0, 'f', 2));
                // Re-emit envelope; correction now includes the temp term.
                const TinySaLevelEnvelope env =
                    envelopeForFreq(m_currentCwHz, m_generatorMode, m_deviceType);
                const double corr = correctionAtFreqDb(m_currentCwHz);
                emit levelRangeReady(env.minDbm + m_calOffsets.lowOutput,
                                     env.maxDbm + m_calOffsets.lowOutput - corr);
                return;
            }
        }
    }, 1500);
}

double TinySaDevice::correctionAtFreqDb(quint64 freqHz) const
{
    // Pick the table the firmware would pick. set_output_path() in
    // sa_core.c:208:
    //   f >= MAX_LOW_OUTPUT_FREQ (1.13 GHz) and mixer_output -> PATH_ULTRA
    //   f >= MAX_LOW_OUTPUT_FREQ        no mixer    -> PATH_LEAKAGE (ADF)
    //   f >  MINIMUM_DIRECT_FREQ (823 MHz)         -> PATH_DIRECT
    //   else                                       -> PATH_LOW
    //
    // The TinySA Ultra defaults setting.mixer_output to on, so above 1.13
    // GHz we go to PATH_ULTRA. (PATH_LEAKAGE is reachable but only when
    // the user explicitly issues `output normal`; until we probe that, the
    // permissive path is the right default.) When out_ultra is empty we
    // fall back to out_adf so this is still defined on Basic.
    const QVector<TinySaCorrectionPoint> *table = &m_outputCorrection;
    double pathOffset = 0.0;
    if (freqHz >= 1130000000ULL) {
        if (!m_outputCorrectionUltra.isEmpty()) {
            table = &m_outputCorrectionUltra;       // PATH_ULTRA, no per-path offset
        } else {
            table = &m_outputCorrectionAdf;
            pathOffset = m_calOffsets.adfOutput;
        }
    } else if (freqHz > 823000000ULL) {
        table = &m_outputCorrectionDirect;
        pathOffset = m_calOffsets.directOutput;
    }
    double raw = interpolateCorrectionTable(*table, freqHz) + pathOffset;
    // Firmware adds a temperature-drift term in set_output_path (sa_core.c:
    // 252-256), but only for non-PATH_LEAKAGE paths.  PATH_LOW, PATH_DIRECT,
    // PATH_ULTRA all include it.  Constants are CENTER_TEMPERATURE=34,
    // DB_PER_DEGREE_ABOVE=0.069, DB_PER_DEGREE_BELOW=0.056. Probed via the
    // hidden `k` command.
    const bool isLeakagePath = (freqHz >= 1130000000ULL
                                && m_outputCorrectionUltra.isEmpty());
    if (m_deviceTempValid && !isLeakagePath) {
        const double dt = m_deviceTempC - 34.0;
        if (dt > 0) raw += dt * 0.069;
        else        raw += dt * 0.056;
    }
    // Firmware rounds the clamp in 0.5 dB steps (sa_core.c:321-322):
    //     a += 0.49;
    //     a = (float)((int)(a*2))/2.0;
    // Mirror that here so the spinbox max lands exactly where the device
    // will silently clamp setting.level.
    const double rounded = std::floor((raw + 0.49) * 2.0) / 2.0;
    // CRITICAL: firmware only does `setting.level -= a` when `a > 0`
    // (sa_core.c:319). When the per-freq correction is negative (i.e. the
    // signal path provides gain at this freq), setting.level is NOT
    // reduced; max stays at level_max(). Clamping at 0 from below mirrors
    // that behaviour. Without this the spinbox max would *raise* above
    // level_max() at frequencies whose correction is negative, which is
    // exactly wrong.
    return std::max(0.0, rounded);
}

void TinySaDevice::probeOutputCorrectionTable()
{
    // Probe all four Low-Output correction tables: out, out_direct, out_adf,
    // out_ultra. Each one is a separate `correction <name>` round-trip; the
    // prompt-paced queue serialises them so the responses do not overlap.
    auto probeOne = [this](const QByteArray &name,
                           QVector<TinySaCorrectionPoint> &dest) {
        enqueueText("correction " + name,
            [this, name, &dest](const QByteArrayList &lines) {
                QVector<TinySaCorrectionPoint> table;
                QRegularExpression re(
                    QString("^correction\\s+%1\\s+(\\d+)\\s+(\\d+)\\s+(-?\\d+(?:\\.\\d+)?)\\s*$")
                        .arg(QString::fromLatin1(name)));
                for (const QByteArray &l : lines) {
                    QString s = QString::fromLatin1(l).trimmed();
                    QRegularExpressionMatch m = re.match(s);
                    if (!m.hasMatch()) continue;
                    TinySaCorrectionPoint p;
                    p.freqHz  = m.captured(2).toULongLong();
                    p.valueDb = m.captured(3).toDouble();
                    table.append(p);
                }
                std::sort(table.begin(), table.end(),
                          [](const TinySaCorrectionPoint &a,
                             const TinySaCorrectionPoint &b){
                              return a.freqHz < b.freqHz;
                          });
                dest = std::move(table);

                QStringList preview;
                for (int i = 0; i < qMin<int>(3, dest.size()); ++i) {
                    preview << QString("%1Hz=%2dB")
                                  .arg(dest[i].freqHz)
                                  .arg(dest[i].valueDb, 0, 'f', 1);
                }
                emit commandResponse(QString("correction %1").arg(QString::fromLatin1(name)),
                    QString("parsed %1 points; first: %2")
                        .arg(dest.size()).arg(preview.join(", ")));

                // Re-emit envelope after the LAST probe (out_adf is last
                // in the chain); doing it per-table is harmless but noisy.
                if (name == "out_adf") {
                    const TinySaLevelEnvelope env =
                        envelopeForFreq(m_currentCwHz, m_generatorMode, m_deviceType);
                    const double corr = correctionAtFreqDb(m_currentCwHz);
                    emit levelRangeReady(
                        env.minDbm + m_calOffsets.lowOutput,
                        env.maxDbm + m_calOffsets.lowOutput - corr);
                }
            }, 5000);
    };

    probeOne("out",        m_outputCorrection);
    probeOne("out_direct", m_outputCorrectionDirect);
    probeOne("out_ultra",  m_outputCorrectionUltra);
    probeOne("out_adf",    m_outputCorrectionAdf);
}

TinySaLevelEnvelope TinySaDevice::envelopeForFreq(quint64 freqHz,
                                                  GeneratorMode mode,
                                                  DeviceType type)
{
    // High Output is rarely reachable (and not at all on Ultra fw 165),
    // so we keep the mode-only constant for it. The real surgical
    // calculation is for Low Output where the firmware uses
    //   actual_max = SL_GENLOW_LEVEL_MAX + low_output_offset
    //   actual_min = SL_GENLOW_LEVEL_MIN + low_output_offset
    // The static methods do not have the offset (it's a per-unit value),
    // so callers add `m_calOffsets.lowOutput` afterwards.
    //
    // Unknown mode is treated as Low Output because that is the default
    // state and the only one this codepath has data for; without this we
    // fall through to a mode-only constant and the per-unit offset never
    // gets applied (which silently allows e.g. -6 dBm at 100 MHz on a unit
    // whose real max is -23.5 dBm).
    if (mode == GeneratorMode::HighOutput) return levelEnvelope(mode);

    const double baseMax = (type == DeviceType::TinySAUltra)
                         ? TinySaFirmware::GenLowLevelMaxUltra
                         : TinySaFirmware::GenLowLevelMaxBasic;
    const double baseMin = (type == DeviceType::TinySAUltra)
                         ? TinySaFirmware::GenLowLevelMinUltra
                         : TinySaFirmware::GenLowLevelMinBasic;
    Q_UNUSED(freqHz);
    // No path-attenuation guess here anymore. The freq-dependent drop the
    // firmware silently applies lives inside correctionAtFreqDb(), which
    // walks the right per-path table the firmware itself uses.
    return { baseMin, baseMax };
}

double TinySaDevice::adcToDbm(quint16 raw) const
{
    return (static_cast<double>(raw) / 32.0) - m_deviceScale;
}

double TinySaDevice::calculateFrequency(int index)
{
    if (m_scanPointsRequested <= 1) return m_scanStartHz;
    return m_scanStartHz
        + (m_scanStopHz - m_scanStartHz) * index / (m_scanPointsRequested - 1);
}

int TinySaDevice::getFirmwareVersion(const QString &versionString)
{
    // Released builds use `tinySA4_v1.4`. Dev builds tag the commit count and
    // hash: `tinySA4_v1.4-177-gabcdef`. Accept both.  F-16
    QRegularExpression re("v\\d+\\.\\d+(?:-(\\d+))?");
    QRegularExpressionMatch m = re.match(versionString);
    if (!m.hasMatch()) return 0;
    QString tag = m.captured(1);
    if (tag.isEmpty()) {
        // Released build without commit suffix. Treat as latest known so we
        // do not fall back to fw < 177 scan branch on a modern device.
        return 200;
    }
    return tag.toInt();
}

void TinySaDevice::detectDeviceType(const QString &versionString)
{
    // Use `contains` rather than `startsWith`. After a reconnect the first
    // couple of bytes can be lost from the version response (port buffer
    // race), which leaves us with strings like `nySA4_v1.4-224-...`. The
    // discriminant we actually care about is the `SA4` substring.
    if (versionString.contains(QStringLiteral("SA4"))) {
        m_deviceType = DeviceType::TinySAUltra;
        m_deviceScale = 174.0;
    } else if (versionString.contains(QStringLiteral("SA"))) {
        m_deviceType = DeviceType::TinySABasic;
        m_deviceScale = 128.0;
    } else {
        m_deviceType = DeviceType::Unknown;
    }
}

// =============================================================================
//  Free-form command (console line)
// =============================================================================

void TinySaDevice::sendCommand(const QString &cmd)
{
    enqueueFireAndForget(cmd.toLatin1());
}

// =============================================================================
//  Identification, lifecycle
// =============================================================================

void TinySaDevice::getVersion()
{
    CommandTransaction tx;
    tx.wire = "version";
    tx.kind = CommandTransaction::Kind::Text;
    tx.timeoutMs = 750;
    tx.label = "version";
    tx.onText = [this](const QByteArrayList &lines) {
        if (lines.isEmpty()) return;
        QString versionStr = QString::fromLatin1(lines.first());
        detectDeviceType(versionStr);
        m_firmwareVersion = getFirmwareVersion(versionStr);
        m_deviceReady = true;
        m_handshakeRetries = 0;
        emit versionInfoReady(versionStr);
        // Per user request: pull the current sweep state right after the
        // handshake so the UI has a starting reference. The prompt-paced
        // queue makes this safe to chain here. Also probe the level range
        // so the UI starts with the real per-mode/per-freq envelope.
        // `probeLevelOffsets` is the important one: it reads the per-unit
        // `low_level_output_offset` which we then plug into the firmware
        // formula. Without it the spinbox would clamp to the firmware's
        // hardcoded -76..-6 baseline, which is wrong for every unit that
        // has any calibration.
        probeLevelOffsets();
        probeOutputCorrectionTable();
        probeDeviceTemperature();
        getSweepSettings(); probeDeviceTemperature();
        probeLevelRange();
    };
    pushTransaction(std::move(tx));
}

void TinySaDevice::getInfo()
{
    enqueueText("info", [this](const QByteArrayList &lines) {
        QStringList all;
        for (const auto &l : lines) all << QString::fromLatin1(l);
        emit deviceInfoReady(all.join('\n'));
    }, 3000);
}

void TinySaDevice::getHelp()
{
    enqueueText("help", [this](const QByteArrayList &lines) {
        QStringList all;
        for (const auto &l : lines) all << QString::fromLatin1(l);
        emit commandResponse("help", all.join('\n'));
    }, 3000);
}

void TinySaDevice::getBatteryVoltage()
{
    enqueueText("vbat", [this](const QByteArrayList &lines) {
        for (const QByteArray &l : lines) {
            QRegularExpression re("(\\d+)\\s*mV");
            QRegularExpressionMatch m = re.match(QString::fromLatin1(l));
            if (m.hasMatch()) {
                emit batteryVoltageReady(m.captured(1).toDouble() / 1000.0);
                return;
            }
        }
    });
}

void TinySaDevice::resetDevice()
{
    // reset does not return a prompt; the device restarts. Send raw, drop
    // any in-flight state.
    writeData("reset\r\n");
    if (m_inFlightValid) completeInFlight();
    m_deviceReady = false;
}

void TinySaDevice::clearConfig() { enqueueFireAndForget("clearconfig 1234"); }

// =============================================================================
//  Frequency control
// =============================================================================

// Frequency/sweep setters always chain a sweep-readback so the UI gets a
// confirmed reference of what the device actually accepted. The chained
// `sweep` transaction runs after the setter's `ch> ` because the queue is
// prompt-paced; we cannot intermix.
void TinySaDevice::setFrequency(double freq_hz) {
    enqueueFireAndForget(QString("freq %1").arg(freq_hz, 0, 'f', 0).toLatin1());
    getSweepSettings(); probeDeviceTemperature();
    probeLevelRange();
}
void TinySaDevice::setSweep(double start_hz, double stop_hz) {
    enqueueFireAndForget(QString("sweep %1 %2").arg(start_hz, 0, 'f', 0).arg(stop_hz, 0, 'f', 0).toLatin1());
    getSweepSettings(); probeDeviceTemperature();
    probeLevelRange();
}
void TinySaDevice::setSweepStart(double freq_hz) {
    enqueueFireAndForget(QString("sweep start %1").arg(freq_hz, 0, 'f', 0).toLatin1());
    getSweepSettings(); probeDeviceTemperature();
    probeLevelRange();
}
void TinySaDevice::setSweepStop(double freq_hz) {
    enqueueFireAndForget(QString("sweep stop %1").arg(freq_hz, 0, 'f', 0).toLatin1());
    getSweepSettings(); probeDeviceTemperature();
    probeLevelRange();
}
void TinySaDevice::setSweepCenter(double freq_hz) {
    enqueueFireAndForget(QString("sweep center %1").arg(freq_hz, 0, 'f', 0).toLatin1());
    getSweepSettings(); probeDeviceTemperature();
    probeLevelRange();
}
void TinySaDevice::setSweepSpan(double span_hz) {
    enqueueFireAndForget(QString("sweep span %1").arg(span_hz, 0, 'f', 0).toLatin1());
    getSweepSettings(); probeDeviceTemperature();
    probeLevelRange();
}
void TinySaDevice::setSweepCW(double freq_hz) {
    enqueueFireAndForget(QString("sweep cw %1").arg(freq_hz, 0, 'f', 0).toLatin1());
    getSweepSettings(); probeDeviceTemperature();
    probeLevelRange();
}
void TinySaDevice::probeLevelOffsets()
{
    enqueueText("leveloffset", [this](const QByteArrayList &lines) {
        // Real device output (Ultra fw 165) writes `low output` with a SPACE,
        // not `low_output` with underscore as my earlier reading of the
        // source suggested. Match both. Also handle `direct output`,
        // `high output`. We key off the full token sequence between
        // `leveloffset` and the trailing number.
        for (const QByteArray &l : lines) {
            QString s = QString::fromLatin1(l).trimmed();
            if (!s.startsWith(QStringLiteral("leveloffset"))) continue;
            // Drop the leading word, the rest is `NAME [output] VALUE`.
            QString rest = s.mid(QStringLiteral("leveloffset").size()).trimmed();
            // Pull the trailing number off.
            QRegularExpression numRe("\\s+(-?\\d+(?:\\.\\d+)?)\\s*$");
            QRegularExpressionMatch nm = numRe.match(rest);
            if (!nm.hasMatch()) continue;
            const double value = nm.captured(1).toDouble();
            const QString key = rest.left(nm.capturedStart()).trimmed().toLower();
            // Now `key` is "low output", "low_output", "low", "direct output",
            // "high output", etc.
            if (key == "low output" || key == "low_output") {
                m_calOffsets.lowOutput = value;
            } else if (key == "direct output" || key == "direct_output") {
                m_calOffsets.directOutput = value;
            } else if (key == "high output" || key == "high_output") {
                m_calOffsets.highOutput = value;
            } else if (key == "adf") {
                m_calOffsets.adfOutput = value;   // config.adf_level_offset
            }
        }
        m_calOffsets.valid = true;
        // Surface what we parsed so it's visible in the tester console.
        // Specifically the low_output value is the one the freq-banded
        // envelope formula uses. If this prints "0.0" the parser didn't
        // see the expected line and we need to look at the raw output.
        emit commandResponse(
            "leveloffset",
            QString("parsed: low_output=%1 dB, direct_output=%2 dB, high_output=%3 dB")
                .arg(m_calOffsets.lowOutput, 0, 'f', 1)
                .arg(m_calOffsets.directOutput, 0, 'f', 1)
                .arg(m_calOffsets.highOutput, 0, 'f', 1));

        // Now that we have the offset, re-emit the envelope. Use the
        // confirmed CW frequency if we have one (sweep readback already
        // landed), otherwise fall back to the freq the user has in mind.
        // 0 Hz also flows through envelopeForFreq fine, just hits the
        // PATH_LOW band.
        const quint64 hz = m_currentCwHz; // may be 0 if sweep not back yet
        const TinySaLevelEnvelope env =
            envelopeForFreq(hz, m_generatorMode, m_deviceType);
        // Per-freq correction from the `correction out` table. Until that
        // probe lands, m_outputCorrection is empty and correctionAtFreqDb()
        // returns 0 - matching the firmware behaviour when
        // setting.disable_correction is true.
        const double corr = correctionAtFreqDb(hz);
        emit levelRangeReady(
            env.minDbm + m_calOffsets.lowOutput,
            env.maxDbm + m_calOffsets.lowOutput - corr);
    }, 3000);
}

void TinySaDevice::probeLevelRange()
{
    // Bare `level` returns `usage: level X..Y` where X..Y is the range that
    // is **currently** valid for the active mode and frequency. The range
    // moves around as freq and mode change, so we keep re-probing.
    //
    // The usage line normally rides handleTextChunk's reject path; setting
    // onReject here suppresses commandRejected emission and parses out the
    // numbers instead.
    CommandTransaction tx;
    tx.wire = "level";
    tx.kind = CommandTransaction::Kind::Text;
    tx.timeoutMs = 1500;
    tx.label = "level";
    tx.onReject = [this](const QByteArray &line) {
        QRegularExpression re("usage:\\s*level\\s*(-?\\d+(?:\\.\\d+)?)\\s*\\.\\.\\s*(-?\\d+(?:\\.\\d+)?)");
        QRegularExpressionMatch m = re.match(QString::fromLatin1(line));
        if (!m.hasMatch()) return;
        double lo = m.captured(1).toDouble();
        double hi = m.captured(2).toDouble();
        // The probe's lo/hi are the firmware's hardcoded baseline (no
        // per-unit offset). Intersect with the freq-banded calculation
        // (which has the offset baked in) so the probe never widens us
        // back to a too-generous range above 823 MHz, and never opens us
        // up past the per-unit `low_level_output_offset` ceiling that the
        // firmware silently clamps to.
        if (m_currentCwHz > 0) {
            const TinySaLevelEnvelope band = envelopeForFreq(
                m_currentCwHz, m_generatorMode, m_deviceType);
            const double bandLo = band.minDbm + m_calOffsets.lowOutput;
            const double bandHi = band.maxDbm + m_calOffsets.lowOutput
                                  - correctionAtFreqDb(m_currentCwHz);
            lo = qMax(lo, bandLo);
            hi = qMin(hi, bandHi);
        }
        emit levelRangeReady(lo, hi);
    };
    pushTransaction(std::move(tx));
}

void TinySaDevice::getSweepSettings()
{
    // `sweep` with no args returns current settings. Empirically (TinySA
    // Basic, fw observed on this unit) the format is a single line:
    //     `{start_hz} {stop_hz} {points}`
    // Future firmware variants may produce a multi-line block; in that case
    // we still emit the raw text via commandResponse so nothing is lost.
    enqueueText("sweep", [this](const QByteArrayList &lines) {
        QStringList all;
        for (const auto &l : lines) all << QString::fromLatin1(l);
        emit commandResponse("sweep", all.join('\n'));

        // Try to parse the single-line `start stop points` form for
        // structured callers (the tester / future calibration workflow).
        for (const QByteArray &l : lines) {
            QList<QByteArray> toks = l.simplified().split(' ');
            if (toks.size() < 3) continue;
            bool okStart, okStop, okPts;
            double start = QString::fromLatin1(toks[0]).toDouble(&okStart);
            double stop  = QString::fromLatin1(toks[1]).toDouble(&okStop);
            int    pts   = QString::fromLatin1(toks[2]).toInt(&okPts);
            if (okStart && okStop && okPts) {
                emit sweepSettingsReady(start, stop, pts);
                // Track the confirmed CW so probeLevelRange can intersect
                // the firmware-side mode-only range with the freq band.
                if (qFuzzyCompare(start, stop) && start >= 0) {
                    m_currentCwHz = static_cast<quint64>(start);
                    const TinySaLevelEnvelope env = envelopeForFreq(
                        m_currentCwHz, m_generatorMode, m_deviceType);
                    // The firmware applies per-freq correction inside
                    // set_output_path via get_frequency_correction(); we
                    // mirror it from the `correction out` table.
                    emit levelRangeReady(
                        env.minDbm + m_calOffsets.lowOutput,
                        env.maxDbm + m_calOffsets.lowOutput
                          - correctionAtFreqDb(m_currentCwHz));
                } else {
                    m_currentCwHz = 0; // sweep range, freq-band undefined
                }
                return;
            }
        }
    }, 1500);
}

void TinySaDevice::getFrequencies()
{
    enqueueText("frequencies", [this](const QByteArrayList &lines) {
        QVector<double> freqs;
        for (const QByteArray &l : lines) {
            for (const QByteArray &tok : l.split(' ')) {
                bool ok;
                double v = QString::fromLatin1(tok).toDouble(&ok);
                if (ok) freqs.append(v);
            }
        }
        if (!freqs.isEmpty()) emit frequenciesReady(freqs);
    }, 3000);
}

// =============================================================================
//  Measurement settings
// =============================================================================

void TinySaDevice::setRbw(const QString &rbw)   { enqueueFireAndForget(QString("rbw %1").arg(rbw).toLatin1()); }
void TinySaDevice::setRbw(int rbw_khz)          { enqueueFireAndForget(QString("rbw %1").arg(rbw_khz).toLatin1()); }
void TinySaDevice::setAttenuation(const QString &atten) { enqueueFireAndForget(QString("attenuate %1").arg(atten).toLatin1()); }
void TinySaDevice::setAttenuation(int att_db)   { enqueueFireAndForget(QString("attenuate %1").arg(att_db).toLatin1()); }
void TinySaDevice::setLna(bool enabled)         { enqueueFireAndForget(QString("lna %1").arg(enabled ? "on" : "off").toLatin1()); }
void TinySaDevice::setSpurReduction(const QString &mode) { enqueueFireAndForget(QString("spur %1").arg(mode).toLatin1()); }
void TinySaDevice::setSampleRepeat(int count)   { enqueueFireAndForget(QString("repeat %1").arg(count).toLatin1()); }
void TinySaDevice::setIF(double freq_hz)
{
    if (freq_hz == 0) enqueueFireAndForget("if 0");
    else              enqueueFireAndForget(QString("if %1").arg(freq_hz, 0, 'f', 0).toLatin1());
}

// =============================================================================
//  Measurement modes
// =============================================================================

void TinySaDevice::setMeasurementMode(const QString &mode) {
    enqueueFireAndForget(QString("calc %1").arg(mode).toLatin1());
}
void TinySaDevice::pauseSweep()  { enqueueFireAndForget("pause"); }
void TinySaDevice::resumeSweep() { enqueueFireAndForget("resume"); }
void TinySaDevice::getStatus()
{
    enqueueText("status", [this](const QByteArrayList &lines) {
        if (lines.isEmpty()) return;
        emit commandResponse("status", QString::fromLatin1(lines.first()));
    });
}

// =============================================================================
//  Display
// =============================================================================

void TinySaDevice::setUnit(const QString &unit)        { enqueueFireAndForget(QString("trace %1").arg(unit).toLatin1()); }
void TinySaDevice::setScale(const QString &scale)      { enqueueFireAndForget(QString("trace scale %1").arg(scale).toLatin1()); }
void TinySaDevice::setRefLevel(const QString &level)   { enqueueFireAndForget(QString("trace reflevel %1").arg(level).toLatin1()); }
void TinySaDevice::setZeroLevel(double level)          { enqueueFireAndForget(QString("zero %1").arg(level).toLatin1()); }

// =============================================================================
//  Marker
// =============================================================================

void TinySaDevice::setMarker(int id, const QString &setting) {
    enqueueFireAndForget(QString("marker %1 %2").arg(id).arg(setting).toLatin1());
}
void TinySaDevice::getMarker(int id)
{
    enqueueText(QString("marker %1").arg(id).toLatin1(),
        [this, id](const QByteArrayList &lines) {
            for (const QByteArray &l : lines) {
                QRegularExpression re("M(\\d+)\\s+(\\d+)Hz\\s+([+-]?\\d+\\.?\\d*)dBm");
                QRegularExpressionMatch m = re.match(QString::fromLatin1(l));
                if (m.hasMatch()) {
                    int mid = m.captured(1).toInt();
                    double freq = m.captured(2).toDouble();
                    double lvl = m.captured(3).toDouble();
                    emit markerDataReady(mid, freq, lvl);
                    return;
                }
            }
            Q_UNUSED(id);
        });
}
void TinySaDevice::getMarkerPeak(int id) { enqueueFireAndForget(QString("marker %1 peak").arg(id).toLatin1()); }

// =============================================================================
//  Trace
// =============================================================================

void TinySaDevice::getTraceData(int traceId)
{
    enqueueText(QString("data %1").arg(traceId).toLatin1(),
        [this, traceId](const QByteArrayList &lines) {
            // F-10 tolerant parse: each line may be one float (just level) or
            // two floats (freq level). Accept whichever the firmware sends.
            QVector<double> freqs;
            QVector<double> levels;
            for (const QByteArray &l : lines) {
                QList<QByteArray> toks = l.split(' ');
                QVector<double> nums;
                for (const QByteArray &t : toks) {
                    if (t.isEmpty()) continue;
                    bool ok;
                    double v = QString::fromLatin1(t).toDouble(&ok);
                    if (ok) nums.append(v);
                }
                if (nums.size() == 1) {
                    levels.append(nums[0]);
                } else if (nums.size() >= 2) {
                    freqs.append(nums[0]);
                    levels.append(nums[1]);
                }
            }
            if (!levels.isEmpty()) {
                emit traceDataReady(traceId, freqs, levels);
            }
        }, 3000);
}
void TinySaDevice::storeTrace()    { enqueueFireAndForget("trace store"); }
void TinySaDevice::clearTrace()    { enqueueFireAndForget("trace clear"); }
void TinySaDevice::subtractTrace() { enqueueFireAndForget("trace subtract"); }

// =============================================================================
//  Trigger
// =============================================================================

void TinySaDevice::setTrigger(const QString &mode) { enqueueFireAndForget(QString("trigger %1").arg(mode).toLatin1()); }
void TinySaDevice::setTriggerLevel(double level)   { enqueueFireAndForget(QString("trigger %1").arg(level).toLatin1()); }

// =============================================================================
//  Generator
// =============================================================================

void TinySaDevice::setMode(const QString &range, const QString &dir) {
    // F-21: enforce the two-token form `mode (low|high) (input|output)`.
    enqueueFireAndForget(QString("mode %1 %2").arg(range, dir).toLatin1());
    if (dir.toLower() == "output") {
        if (range.toLower() == "high") {
            m_generatorMode = GeneratorMode::HighOutput;
            const TinySaLevelEnvelope env = levelEnvelope(m_generatorMode);
            emit levelRangeReady(env.minDbm, env.maxDbm);
        } else if (range.toLower() == "low") {
            m_generatorMode = GeneratorMode::LowOutput;
            const TinySaLevelEnvelope env = levelEnvelope(m_generatorMode);
            emit levelRangeReady(env.minDbm, env.maxDbm);
        }
    }
}
void TinySaDevice::setGeneratorFrequency(double freq_hz) {
    enqueueFireAndForget(QString("sweep center %1").arg(freq_hz, 0, 'f', 0).toLatin1());
    getSweepSettings(); probeDeviceTemperature();
    probeLevelRange();
}
void TinySaDevice::setGeneratorFrequencyRange(const QString &range) {
    // `mode` does not appear in the `sweep` readback string, so we can't
    // chain a sweep readback to confirm the mode change itself. But level
    // range definitely shifts with mode, so re-probe it. A usage: rejection
    // (e.g. `mode high output` on fw 165) is surfaced via commandRejected.
    //
    // Track the intent locally and emit the firmware-source-derived envelope
    // immediately so the UI clamp lands before the probe round-trip. The
    // probe still runs afterwards and refines if calibration data shifted
    // the actual achievable range.
    const QString r = range.toLower();
    if (r == "high") m_generatorMode = GeneratorMode::HighOutput;
    else if (r == "low") m_generatorMode = GeneratorMode::LowOutput;
    enqueueFireAndForget(QString("mode %1 output").arg(range).toLatin1());
    const TinySaLevelEnvelope env = levelEnvelope(m_generatorMode);
    emit levelRangeReady(
        env.minDbm + m_calOffsets.lowOutput,
        env.maxDbm + m_calOffsets.lowOutput - correctionAtFreqDb(m_currentCwHz));
    probeLevelRange();
}
void TinySaDevice::setGeneratorOutput(bool on) {
    enqueueFireAndForget(QString("output %1").arg(on ? "on" : "off").toLatin1());
}
void TinySaDevice::setGeneratorLevel(double level_dbm) {
    // Probe temperature first so the firmware-side temp at level-set time
    // matches what our formula sees. Without this, ambient self-heating
    // between freq change and the actual level write can flip a 0.5 dB
    // rounding bin in set_output_path's clamp. The probe is prompt-paced,
    // so its response lands before `level X` goes out on the wire.
    probeDeviceTemperature();
    enqueueFireAndForget(QString("level %1").arg(level_dbm).toLatin1());
}
void TinySaDevice::setGeneratorModulation(const QString &type) {
    enqueueFireAndForget(QString("modulation %1").arg(type.toLower()).toLatin1());
}
void TinySaDevice::setGeneratorModulation(const QString &type, int value)
{
    // F-22: documented forms are off|AM_1kHz|AM_10Hz|NFM|WFM|extern. The
    // depth/deviation/freq sub-commands are [hidden, quicktinysa]; chain them
    // via the queue so the head transaction's `ch> ` paces the next one.
    const QString typeLower = type.toLower();
    if (typeLower == "off") {
        enqueueFireAndForget("modulation off");
        return;
    }
    if (typeLower == "am") {
        enqueueFireAndForget("modulation am");
        if (value > 0 && value <= 100) {
            enqueueFireAndForget(QString("modulation depth %1").arg(value).toLatin1());
        }
        return;
    }
    if (typeLower == "fm") {
        enqueueFireAndForget("modulation fm");
        if (value >= 100 && value <= 6000) {
            enqueueFireAndForget(QString("modulation deviation %1").arg(value).toLatin1());
        }
        return;
    }
    if (typeLower == "freq") {
        if (value >= 100 && value <= 6000) {
            enqueueFireAndForget(QString("modulation freq %1").arg(value).toLatin1());
        }
        return;
    }
    enqueueFireAndForget(QString("modulation %1").arg(type).toLatin1());
}

void TinySaDevice::setLevelChange(double delta_db) { enqueueFireAndForget(QString("levelchange %1").arg(delta_db).toLatin1()); }
void TinySaDevice::setSweepTime(double seconds)    { enqueueFireAndForget(QString("sweeptime %1").arg(seconds).toLatin1()); }
void TinySaDevice::setSweepVoltage(double voltage) { enqueueFireAndForget(QString("sweep_voltage %1").arg(voltage).toLatin1()); }

// =============================================================================
//  Calibration helpers
// =============================================================================

void TinySaDevice::setCalOutput(const QString &freq)  { enqueueFireAndForget(QString("caloutput %1").arg(freq).toLatin1()); }
void TinySaDevice::setExternalGain(double gain_db)    { enqueueFireAndForget(QString("ext_gain %1").arg(gain_db).toLatin1()); }
void TinySaDevice::setActualFreq(double freq_hz)      { enqueueFireAndForget(QString("actual_freq %1").arg(freq_hz, 0, 'f', 0).toLatin1()); }
void TinySaDevice::setDac(int value)                  { enqueueFireAndForget(QString("dac %1").arg(value).toLatin1()); }
void TinySaDevice::setVbatOffset(int offset)          { enqueueFireAndForget(QString("vbat_offset %1").arg(offset).toLatin1()); }
void TinySaDevice::setLevelOffset(const QString &type, const QString &mode, double error) {
    enqueueFireAndForget(QString("leveloffset %1 %2 %3").arg(type, mode).arg(error).toLatin1());
}

// =============================================================================
//  Correction tables
// =============================================================================

void TinySaDevice::getCorrection(const QString &mode)
{
    enqueueText(QString("correction %1").arg(mode).toLatin1(),
        [this, mode](const QByteArrayList &lines) {
            QVector<QVector<double>> table;
            for (const QByteArray &l : lines) {
                QVector<double> row;
                for (const QByteArray &t : l.split(' ')) {
                    if (t.isEmpty()) continue;
                    bool ok;
                    double v = QString::fromLatin1(t).toDouble(&ok);
                    if (ok) row.append(v);
                }
                if (!row.isEmpty()) table.append(row);
            }
            if (!table.isEmpty()) emit correctionTableReady(mode, table);
        }, 3000);
}
void TinySaDevice::setCorrection(const QString &mode, int entry, double freq_hz, double level_db) {
    enqueueFireAndForget(QString("correction %1 %2 %3 %4")
        .arg(mode).arg(entry).arg(freq_hz, 0, 'f', 0).arg(level_db).toLatin1());
}

// =============================================================================
//  Colours / device ID
// =============================================================================

void TinySaDevice::getColors() { enqueueFireAndForget("color"); }
void TinySaDevice::setColor(int id, uint32_t rgb) {
    enqueueFireAndForget(QString("color %1 0x%2").arg(id).arg(rgb, 6, 16, QChar('0')).toLatin1());
}
void TinySaDevice::getDeviceId()        { enqueueFireAndForget("deviceid"); }
void TinySaDevice::setDeviceId(int id)  { enqueueFireAndForget(QString("deviceid %1").arg(id).toLatin1()); }

// =============================================================================
//  Scanning
// =============================================================================

void TinySaDevice::startScan(double start_hz, double stop_hz, int points)
{
    if (!m_deviceReady) {
        qWarning() << "startScan before deviceReady, rejecting";
        emit commandRejected("scan", "device not ready");
        return;
    }
    m_scanStartHz = start_hz;
    m_scanStopHz = stop_hz;
    m_scanPointsRequested = points;

    // ASCII scan returns "freq level" pairs per point with outmask 3.
    enqueueText(QString("scan %1 %2 %3 3")
                    .arg(start_hz, 0, 'f', 0).arg(stop_hz, 0, 'f', 0).arg(points).toLatin1(),
        [this](const QByteArrayList &lines) {
            QVector<double> freqs;
            QVector<double> levels;
            for (const QByteArray &l : lines) {
                QList<QByteArray> toks = l.split(' ');
                if (toks.size() < 2) continue;
                bool ok1, ok2;
                double f = QString::fromLatin1(toks[0]).toDouble(&ok1);
                double v = QString::fromLatin1(toks[1]).toDouble(&ok2);
                if (ok1 && ok2) { freqs.append(f); levels.append(v); }
            }
            if (!levels.isEmpty()) emit sweepDataReady(freqs, levels);
        }, 30000);
}

void TinySaDevice::startScanRaw(double start_hz, double stop_hz, int points)
{
    // F-07: option 0 = one-shot, portable across firmware. Continuous use
    // startContinuousScanRaw.
    if (!m_deviceReady) {
        qWarning() << "startScanRaw before deviceReady, rejecting";
        emit commandRejected("scanraw", "device not ready");
        return;
    }
    m_scanStartHz = start_hz;
    m_scanStopHz = stop_hz;
    m_continuousScanActive = false;
    startScanFramer(points);

    CommandTransaction tx;
    tx.wire = QString("scanraw %1 %2 %3 0").arg(start_hz, 0, 'f', 0).arg(stop_hz, 0, 'f', 0).arg(points).toLatin1();
    tx.kind = CommandTransaction::Kind::BinaryContinuous;
    tx.timeoutMs = 30000;
    tx.label = "scanraw";
    tx.onBinaryContinuous = [this](QByteArray &buf) -> bool {
        bool framerWantsEnd = feedScanFramer(buf);
        if (framerWantsEnd) {
            int promptIdx = buf.indexOf("ch> ");
            if (promptIdx >= 0) buf.remove(0, promptIdx + 4);
            return true;
        }
        return false;
    };
    pushTransaction(std::move(tx));
}

void TinySaDevice::startContinuousScanRaw(double start_hz, double stop_hz, int points)
{
    if (m_deviceType != DeviceType::TinySAUltra || m_firmwareVersion < 177) {
        qWarning() << "Continuous scanraw needs Ultra fw >= 177; falling back to one-shot";
        startScanRaw(start_hz, stop_hz, points);
        return;
    }
    m_scanStartHz = start_hz;
    m_scanStopHz = stop_hz;
    m_continuousScanActive = true;
    startScanFramer(points);

    CommandTransaction tx;
    tx.wire = QString("scanraw %1 %2 %3 3").arg(start_hz, 0, 'f', 0).arg(stop_hz, 0, 'f', 0).arg(points).toLatin1();
    tx.kind = CommandTransaction::Kind::BinaryContinuous;
    tx.timeoutMs = -1; // continuous; abort is required to end it
    tx.label = "scanraw-continuous";
    tx.onBinaryContinuous = [this](QByteArray &buf) -> bool {
        bool framerHitTerminator = feedScanFramer(buf);
        if (framerHitTerminator) {
            // In continuous mode the only way the framer hits a lone `}` is
            // jog/touch interrupt or an abort.
            m_continuousScanActive = false;
            emit scanInterrupted();
            int promptIdx = buf.indexOf("ch> ");
            if (promptIdx >= 0) buf.remove(0, promptIdx + 4);
            return true;
        }
        return false;
    };
    pushTransaction(std::move(tx));
}

void TinySaDevice::startScanWithOutput(double start_hz, double stop_hz, int points, int outmask)
{
    m_scanStartHz = start_hz;
    m_scanStopHz = stop_hz;
    enqueueText(QString("scan %1 %2 %3 %4")
                    .arg(start_hz, 0, 'f', 0).arg(stop_hz, 0, 'f', 0).arg(points).arg(outmask).toLatin1(),
        nullptr, 30000);
}

void TinySaDevice::stopScan()
{
    // Issue abort as a normal transaction; once its `ch> ` arrives we are
    // idle again. The Ultra fw >= 177 abort happens on the wire as
    // `abort\r\n`; older firmware just stops on any input. Either way an
    // enqueued transaction is the right shape now that we are prompt-paced.
    if (!m_continuousScanActive && !m_inFlightValid) return;
    m_continuousScanActive = false;
    enqueueFireAndForget("abort");
}

void TinySaDevice::abort()                { enqueueFireAndForget("abort"); }
void TinySaDevice::setAbort(bool enabled) { enqueueFireAndForget(QString("abort %1").arg(enabled ? "on" : "off").toLatin1()); }

// =============================================================================
//  Screen capture
// =============================================================================

void TinySaDevice::captureScreen()
{
    CommandTransaction tx;
    tx.wire = "capture";
    tx.kind = CommandTransaction::Kind::BinaryFixed;
    tx.expectedBytes = kScreenBytes;
    tx.timeoutMs = 5000;
    tx.label = "capture";
    tx.onBinaryFixed = [this](const QByteArray &block) {
        emit screenCaptureReady(block);
    };
    pushTransaction(std::move(tx));
}

// =============================================================================
//  Touch
// =============================================================================

void TinySaDevice::touchScreen(int x, int y) { enqueueFireAndForget(QString("touch %1 %2").arg(x).arg(y).toLatin1()); }
void TinySaDevice::releaseTouch()             { enqueueFireAndForget("release"); }
void TinySaDevice::startTouchCalibration()    { enqueueFireAndForget("touchcal"); }
void TinySaDevice::startTouchTest()           { enqueueFireAndForget("touchtest"); }

// =============================================================================
//  Misc
// =============================================================================

void TinySaDevice::selfTest(int testId) { enqueueText(QString("selftest 0 %1").arg(testId).toLatin1(), nullptr, 30000); }
void TinySaDevice::setRefresh(bool en)  { enqueueFireAndForget(QString("refresh %1").arg(en ? "on" : "off").toLatin1()); }
void TinySaDevice::example()            { enqueueFireAndForget("example"); }
void TinySaDevice::threads()
{
    enqueueText("threads", [this](const QByteArrayList &lines) {
        QStringList all;
        for (const auto &l : lines) all << QString::fromLatin1(l);
        emit commandResponse("threads", all.join('\n'));
    });
}

// =============================================================================
//  SD card (Ultra only)
// =============================================================================

void TinySaDevice::listSDCard()
{
    if (m_deviceType != DeviceType::TinySAUltra) {
        emit rawDeviceOutput("SD card functions only available on TinySA Ultra");
        return;
    }
    enqueueText("sd_list", [this](const QByteArrayList &lines) {
        QStringList all;
        for (const auto &l : lines) all << QString::fromLatin1(l);
        emit commandResponse("sd_list", all.join('\n'));
    }, 5000);
}

void TinySaDevice::readSDFile(const QString &filename)
{
    if (m_deviceType != DeviceType::TinySAUltra) {
        emit rawDeviceOutput("SD card functions only available on TinySA Ultra");
        return;
    }
    // sd_read returns 4-byte LE length, then bytes, then prompt. BinaryFixed
    // can't express the variable length cleanly; left as a TODO. For now,
    // fire and forget so the wire command goes out and the response shows
    // up in the raw output.
    enqueueFireAndForget(QString("sd_read %1").arg(filename).toLatin1());
}

// =============================================================================
//  Time (Ultra only)
// =============================================================================

void TinySaDevice::setTime(const QDateTime &datetime)
{
    if (m_deviceType != DeviceType::TinySAUltra) {
        emit rawDeviceOutput("Time functions only available on TinySA Ultra");
        return;
    }
    // F-26: BCD-style packing: each two-digit decimal date number is written
    // inside a `0x` literal so the firmware reads it via hex-as-decimal.
    // Year is (year - 2000) formatted as two decimal digits (NOT hex). Match
    // QtTinySA's encoding.
    int yr = datetime.date().year() - 2000;
    QString cmd = QString("time b 0x%1%2%3 0x%4%5%6")
        .arg(yr, 2, 10, QChar('0'))
        .arg(datetime.date().month(), 2, 10, QChar('0'))
        .arg(datetime.date().day(),   2, 10, QChar('0'))
        .arg(datetime.time().hour(),  2, 10, QChar('0'))
        .arg(datetime.time().minute(),2, 10, QChar('0'))
        .arg(datetime.time().second(),2, 10, QChar('0'));
    enqueueFireAndForget(cmd.toLatin1());
}
