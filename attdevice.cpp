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
            emit detectedDevice("Unsupported Device", step(), max(), format());
        }
}

void AttDevice::tryCurrentProbe()
{
    qDebug()<<Q_FUNC_INFO<<m_probeTypeIdx;
    m_probeTimer.stop();
    if (m_probeTypeIdx >= int(sizeof(deviceTypes) / sizeof(DeviceType)))
        {
            finishProbe(false);
            return;
        }
    const DeviceType &dev = deviceTypes[m_probeTypeIdx];
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
            const DeviceType &dev = deviceTypes[m_probeTypeIdx];
            qDebug()<<"finishProbe: model="<<dev.model<<"format="<<formatToString(dev.format);
            setModel(dev.model);
            setStep(dev.step);
            setMax(dev.max);
            setCurrentValue(dev.max);
            setFormat(formatToString(dev.format));
            emit detectedDevice(m_model, m_step, m_max, formatToString(m_format));
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
    static QString buffer;
    buffer += line;

    QRegularExpression re(R"(attOK|ATT\s*=\s*-?\d+\.\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = re.globalMatch(buffer);

    int lastEnd = 0;
    while (it.hasNext())
        {
            QRegularExpressionMatch fullMatch = it.next();
            QString message = fullMatch.captured(0);
            lastEnd = fullMatch.capturedEnd();

            qDebug()<<Q_FUNC_INFO<<message;
            QString trimmed = message.trimmed();
            QRegularExpression valueRe("ATT\\s*=\\s*-?([0-9]+\\.[0-9]+)", QRegularExpression::CaseInsensitiveOption);
            auto match = valueRe.match(trimmed);

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
                                            emit detectedDevice(model() + " " + QString::asprintf(formatToString(m_format).toStdString().c_str(), 0), step(), max(), format());
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

    buffer = buffer.mid(lastEnd);

    if (buffer.length() > 4096)
        buffer.clear();
}
