#include "rfpmv7device.h"
#include <QRegularExpression>
#include <QTimer>
#include <QDateTime>

RfpmV7Device::RfpmV7Device(const PMDeviceProperties &props, QObject *parent)
    : AbstractPMDevice(props, parent)
{
    m_serialPort = new SerialPortInterface(this);

    connect(m_serialPort, &SerialPortInterface::serialPortNewData, this, &RfpmV7Device::onSerialPortNewData);
    connect(m_serialPort, &SerialPortInterface::serialPortErrorSignal, this, &RfpmV7Device::onSerialPortError);

    connect(m_serialPort, &SerialPortInterface::portOpened, this, [=](){
        emit deviceConnected();
        m_serialPort->writeData("IC000+L\r\n");
        emit newLogMessage(QString("%1 [DEVICE] Started continuous data stream (L command).")
                               .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz")));
    });

    connect(m_serialPort, &SerialPortInterface::portClosed, this, &AbstractPMDevice::deviceDisconnected);
}

RfpmV7Device::~RfpmV7Device()
{
}

void RfpmV7Device::connectDevice(const QString &portName)
{
    m_serialPort->setportName(portName);
    m_serialPort->setbaudRate(m_properties.baudRate);
    m_serialPort->startPort();
}

void RfpmV7Device::disconnectDevice()
{
    if (m_serialPort->isPortOpen()) {
        m_serialPort->writeData("IC000+T\r\n");
        emit newLogMessage(QString("%1 [DEVICE] Stopped continuous data stream (T command).")
                               .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz")));
    }
    m_serialPort->stopPort();
}

void RfpmV7Device::setFrequency(quint64 freqHz)
{
    m_currentFrequencyHz = freqHz;
    sendWriteCommand();
}

void RfpmV7Device::setOffset(double offsetDb)
{
    m_currentOffsetDb = offsetDb;
    sendWriteCommand();
}

void RfpmV7Device::setInternalAttenuation(double attDb)
{
    m_internalAttDb = attDb;
    emit internalAttenuationChanged(m_internalAttDb);
    sendWriteCommand();
}

void RfpmV7Device::sendWriteCommand()
{
    if (!m_serialPort->isPortOpen()) return;

    QString sign = (m_currentOffsetDb >= 0) ? "+" : "-";
    QString offsetStr = QString::number(qAbs(m_currentOffsetDb), 'f', 1).rightJustified(4, '0');
    QString attStr = QString::number(m_internalAttDb, 'f', 2).rightJustified(5, '0');
    QString freqStr = QString::number(m_currentFrequencyHz / 1000000);

    QString command = QString("IC000+W%1%2+%3+%4\r\n").arg(sign).arg(offsetStr).arg(attStr).arg(freqStr);

    m_serialPort->writeData(command.toLatin1());
    emit newLogMessage(QString("%1 [DEVICE] Sent V7 Write Command: %2")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                           .arg(command.trimmed()));
}

void RfpmV7Device::onSerialPortNewData(const QString &data)
{
    emit rawDataReceived(data);
    processData(data);
}

void RfpmV7Device::processData(const QString &data)
{
    static QString buffer;
    buffer += data;

    QStringList lines = buffer.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
    if (!buffer.endsWith('\n') && !buffer.endsWith('\r')) {
        buffer = lines.isEmpty() ? "" : lines.takeLast();
    } else {
        buffer.clear();
    }

    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.startsWith("Level:")) {
            QRegularExpression re("Level:\\s*(-?[0-9.]+)dBm");
            QRegularExpressionMatch match = re.match(trimmedLine);
            if (match.hasMatch()) {
                m_lastDbm = match.captured(1).toDouble();
                m_waitingForPower = true;
            }
        } else if (trimmedLine.startsWith("Power:") && m_waitingForPower) {
            QRegularExpression re("Power:\\s*([0-9.]+)(\\wW)");
            QRegularExpressionMatch match = re.match(trimmedLine);
            if (match.hasMatch()) {
                const QDateTime timestamp = QDateTime::currentDateTime();
                double powerValue = match.captured(1).toDouble();
                QString unit = match.captured(2);
                double powerMilliwatts = 0.0;

                if (unit == "uW") {
                    powerMilliwatts = powerValue / 1000.0;
                } else if (unit == "nW") {
                    powerMilliwatts = powerValue / 1000000.0;
                } else if (unit == "pW") {
                    powerMilliwatts = powerValue / 1000000000.0;
                } else if (unit == "mW") {
                    powerMilliwatts = powerValue;
                } else if (unit == "W") {
                    powerMilliwatts = powerValue * 1000.0;
                }

                double vpp = UnitConverter::milliwattsToVpp(powerMilliwatts);

                emit measurementReady(timestamp, m_lastDbm, vpp);
                m_waitingForPower = false;
            }
        } else {
            emit newLogMessage(QString("%1 [DEVICE] Non-measurement data: %2")
                                   .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                                   .arg(trimmedLine));
        }
    }
}


void RfpmV7Device::onSerialPortError(const QString &error)
{
    emit deviceError(error);
}
