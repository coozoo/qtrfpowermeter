#include "attdevice.h"

AttDevice::AttDevice(QObject *parent)
    : SerialPortInterface(parent)
{
    connect(this, &SerialPortInterface::serialPortNewData, this, &AttDevice::onSerialPortNewData);
    connect(this, &AttDevice::expectedValueChanged, this, &AttDevice::onExpectedValueChanged);
    connect(this, &SerialPortInterface::portOpened, this, &AttDevice::onDevicePort_started);
    connect(this, &SerialPortInterface::portClosed, this, &AttDevice::onPortClosed);
    connect(&m_probeTimer, &QTimer::timeout, this, &AttDevice::onProbeTimeout);
    connect(this, &AttDevice::unknownDevice, this, &AttDevice::tryUnknownFormat);
    connect(&m_unknownFormatTimer, &QTimer::timeout, this, &AttDevice::tryUnknownFormat);
    m_probeTimer.setSingleShot(true);
    m_unknownFormatTimer.setSingleShot(true);

    m_pollingTimer = new QTimer(this);
    m_pollingTimer->setInterval(hardwareReadIntervalMs);
    connect(m_pollingTimer, &QTimer::timeout, this, &AttDevice::readValue);
}

double AttDevice::insertionLossAt(double freqHz) const
{
    for (const InsertionLossBand &b : m_currentIlBands)
        {
            if (freqHz >= b.freqLowHz && freqHz < b.freqHighHz)
                return b.ilDb;
        }
    return 0.0;
}

void AttDevice::setPollingEnabled(bool enabled)
{
    if (enabled) {
        if (isPortOpen()) {
            m_pollingTimer->start();
            readValue();
        }
    } else {
        m_pollingTimer->stop();
    }
}

void AttDevice::writeValue(double value)
{
    qDebug()<<Q_FUNC_INFO<<value<<"format:"<<formatToString(m_format);
    QString cmd = QString::asprintf(formatToString(m_format).toStdString().c_str(), value);
    qDebug()<<"final command:"<<cmd;
    setExpectedValue(value);
    writeData(cmd.toUtf8());
    m_probeState = ProbeWaitingSetOK;
}

void AttDevice::readValue()
{
    qDebug()<<Q_FUNC_INFO;
    writeData("READ\r\n");
}

void AttDevice::onExpectedValueChanged(double value)
{
    qDebug()<<Q_FUNC_INFO<<value;
}

void AttDevice::onDevicePort_started()
{
    qDebug()<<Q_FUNC_INFO;
    probeDeviceType();
}

void AttDevice::onPortClosed()
{
    qDebug() << Q_FUNC_INFO;
    m_pollingTimer->stop();
    // Drop any half-frame left in the parse buffer; the next session must
    // start clean so a stale "Power: 1.0m"-style tail can't fuse with the
    // new device's first chunk.
    m_buffer.clear();
}

void AttDevice::probeDeviceType()
{
    qDebug()<<Q_FUNC_INFO;
    startProbe();
}

void AttDevice::startProbe()
{
    qDebug()<<Q_FUNC_INFO;
    m_unknownFormatIdx = 0;
    m_probeTypeIdx = 0;
    m_inProbe = true;
    m_isProbingUnknownFormats = false; // Ensure this is false for known probe
    // Discard any leftover bytes from a previous session before we start
    // emitting our own probe queries and waiting on the responses.
    m_buffer.clear();
    tryCurrentProbe();
}

void AttDevice::tryUnknownFormat()
{
    // If probing was successful and stopped, don't continue the timer loop.
    if (!m_isProbingUnknownFormats)
        {
            return;
        }

    if (m_unknownFormatIdx < int(AttFormat::Unknown))
        {
            qDebug()<<"Probing unknown format index:"<<m_unknownFormatIdx;
            setFormat(formatToString(static_cast<AttFormat>(m_unknownFormatIdx)));
            setModel("Unknown Device");
            setStep(0.25);
            setMax(100.0);
            m_probeValue = 8.5;
            writeValue(m_probeValue);

            m_unknownFormatTimer.start(1000);
            m_unknownFormatIdx++;
        }
    else
        {
            // All unknown formats failed
            m_unknownFormatTimer.stop();
            m_isProbingUnknownFormats = false;
            m_maxInputDbm = kFallbackMaxInputDbm;
            m_chip.clear();
            m_currentIlBands = fallbackIlBands();
            emit detectedDevice("Unsupported Device", step(), max(), format(),
                                m_maxInputDbm, m_chip);
        }
}

void AttDevice::tryCurrentProbe()
{
    qDebug()<<Q_FUNC_INFO<<m_probeTypeIdx;
    m_probeTimer.stop();
    const auto &table = deviceTypesTable();
    if (m_probeTypeIdx >= table.size())
        {
            finishProbe(false);
            return;
        }
    const DeviceType &dev = table.at(m_probeTypeIdx);
    setFormat(formatToString(dev.format));
    m_probeValue = dev.max;
    writeValue(m_probeValue);
    m_probeTimer.start(1000);
}

void AttDevice::onProbeTimeout()
{
    qDebug()<<Q_FUNC_INFO<<"timeout for type"<<m_probeTypeIdx;
    m_probeState = ProbeIdle;
    m_probeTypeIdx++;
    tryCurrentProbe();
}

void AttDevice::finishProbe(bool found)
{
    qDebug()<<Q_FUNC_INFO<<(found ? "found" : "not found");
    m_probeTimer.stop();
    m_probeState = ProbeIdle;
    m_inProbe = false;
    if (found)
        {
            const DeviceType &dev = deviceTypesTable().at(m_probeTypeIdx);
            qDebug()<<"finishProbe: model="<<dev.model<<"format="<<formatToString(dev.format);
            setModel(dev.model);
            setStep(dev.step);
            setMax(dev.max);
            setCurrentValue(dev.max);
            setFormat(formatToString(dev.format));
            m_maxInputDbm = dev.maxInputDbm;
            m_chip = dev.chip;
            m_currentIlBands = dev.ilBands;
            emit detectedDevice(m_model, m_step, m_max, formatToString(m_format),
                                m_maxInputDbm, m_chip);
            emit valueSetStatus(true);
        }
    else
        {
            // Start probing for unknown formats
            m_isProbingUnknownFormats = true;
            emit unknownDevice(); // This will call tryUnknownFormat
        }
}

void AttDevice::onSerialPortNewData(const QString &line)
{
    m_buffer += line;

    // Compiled once: pattern is a string literal and this slot runs on
    // every serial chunk (polling + probe bursts). Hoisting saves the
    // pattern-compile cost per call.
    static const QRegularExpression kFrameRe(R"(attOK|ATT\s*=\s*-?\d+\.\d+)",
                                             QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kValueRe("ATT\\s*=\\s*-?([0-9]+\\.[0-9]+)",
                                             QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = kFrameRe.globalMatch(m_buffer);

    int lastEnd = 0;
    while (it.hasNext())
        {
            QRegularExpressionMatch fullMatch = it.next();
            QString message = fullMatch.captured(0);
            lastEnd = fullMatch.capturedEnd();

            qDebug()<<Q_FUNC_INFO<<message;
            QString trimmed = message.trimmed();
            auto match = kValueRe.match(trimmed);

            if (m_probeState == ProbeWaitingSetOK && trimmed.compare("attOK", Qt::CaseInsensitive) == 0)
                {
                    m_probeState = ProbeWaitingValue;
                    readValue();
                    continue;
                }
            if (m_probeState == ProbeWaitingValue && match.hasMatch())
                {
                    double val = match.captured(1).toDouble();
                    if (m_inProbe) // Probing for KNOWN devices
                        {
                            qDebug()<<"PROBE: val="<<val<<"m_probeValue="<<m_probeValue
                                    <<"compare="<<qFuzzyCompare(val + 1, m_probeValue + 1);
                            if (qFuzzyCompare(val + 1, m_probeValue + 1))
                                {
                                    emit currentValueChanged(val);
                                    finishProbe(true); // Found a known device
                                }
                            else
                                {
                                    m_probeTypeIdx++;
                                    tryCurrentProbe();
                                }
                            continue;
                        }
                    else // Normal operation OR probing for UNKNOWN formats
                        {
                            m_currentValue = val;
                            emit currentValueChanged(val);
                            if (qFuzzyCompare(m_expectedValue + 1, val + 1))
                                {
                                    emit valueMatched();
                                    emit valueSetStatus(true);

                                    // If we were probing unknown formats, we just found a working one!
                                    if (m_isProbingUnknownFormats)
                                        {
                                            qDebug()<<"SUCCESS: Found working unknown format!";
                                            m_unknownFormatTimer.stop();
                                            m_isProbingUnknownFormats = false; // Stop the probe loop
                                            m_probeState = ProbeIdle;
                                            // Report the device as "Unknown" but with the working format
                                            m_maxInputDbm = kFallbackMaxInputDbm;
                                            m_chip.clear();
                                            m_currentIlBands = fallbackIlBands();
                                            emit detectedDevice(model() + " " + QString::asprintf(formatToString(m_format).toStdString().c_str(), 0),
                                                                step(), max(), format(),
                                                                m_maxInputDbm, m_chip);
                                        }
                                }
                            else
                                {
                                    emit valueMismatched(m_expectedValue, val);
                                    emit valueSetStatus(false);
                                }

                            if (!m_isProbingUnknownFormats)
                                {
                                    m_probeState = ProbeIdle;
                                }
                            continue;
                        }
                }

            if (m_probeState == ProbeIdle && match.hasMatch())
                {
                    double val = match.captured(1).toDouble();
                    if (!qFuzzyCompare(m_currentValue, val))
                        {
                            m_currentValue = val;
                            emit currentValueChanged(val);
                            if (qFuzzyCompare(m_expectedValue + 1, val + 1))
                                {
                                    emit valueMatched();
                                    emit valueSetStatus(true);
                                }
                            else
                                {
                                    emit valueMismatched(m_expectedValue, val);
                                    if(!m_pollingTimer->isActive())
                                    {
                                         emit valueSetStatus(false);
                                    }
                                }
                        }
                    continue;
                }
        }

    m_buffer = m_buffer.mid(lastEnd);

    if (m_buffer.length() > 4096)
        m_buffer.clear();
}
