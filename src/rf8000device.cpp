#include "rf8000device.h"
#include <QRegularExpression>
#include <QDateTime>

Rf8000Device::Rf8000Device(const PMDeviceProperties &props, QObject *parent)
    : AbstractPMDevice(props, parent)
{
    m_serialPort = new SerialPortInterface(this);
    connect(m_serialPort, &SerialPortInterface::serialPortNewData, this, &Rf8000Device::onSerialPortNewData);
    connect(m_serialPort, &SerialPortInterface::serialPortErrorSignal, this, &Rf8000Device::onSerialPortError);
    connect(m_serialPort, &SerialPortInterface::portOpened, this, &AbstractPMDevice::deviceConnected);
    connect(m_serialPort, &SerialPortInterface::portClosed, this, &AbstractPMDevice::deviceDisconnected);

    m_commandTimer = new QTimer(this);
    m_commandTimer->setSingleShot(true);
    m_commandTimer->setInterval(300);
    connect(m_commandTimer, &QTimer::timeout, this, &Rf8000Device::sendBufferedCommand);
}

Rf8000Device::~Rf8000Device()
{
}

void Rf8000Device::connectDevice(const QString &portName)
{
    m_serialPort->setportName(portName);
    m_serialPort->setbaudRate(m_properties.baudRate);
    m_serialPort->startPort();
}

void Rf8000Device::disconnectDevice()
{
    m_commandTimer->stop();
    m_serialPort->stopPort();
}

void Rf8000Device::setFrequency(quint64 freqHz)
{
    m_currentFrequencyHz = freqHz;
    sendCommand();
}

void Rf8000Device::setOffset(double offsetDb)
{
    m_isPositiveOffset = offsetDb >= 0;
    m_currentOffsetDb = qAbs(offsetDb);
    sendCommand();
}

void Rf8000Device::sendCommand()
{
    if (!m_serialPort->isPortOpen()) return;
    m_commandTimer->start();
}

void Rf8000Device::sendBufferedCommand()
{
    if (!m_serialPort->isPortOpen()) return;

    QString freqStr = QString::number(m_currentFrequencyHz / 1000000).rightJustified(4, '0');
    QString offsetStr = (m_isPositiveOffset ? "+" : "-") + QString::number(m_currentOffsetDb, 'f', 1).rightJustified(4, '0');
    QString command = QString("$%1%2#").arg(freqStr).arg(offsetStr);

    m_serialPort->writeData(command.toLatin1());
    emit newLogMessage(QString("%1 [DEVICE] Sent command: %2")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                           .arg(command));
}

void Rf8000Device::onSerialPortNewData(const QString &data)
{
    emit rawDataReceived(data);
    processData(data);
}

void Rf8000Device::processData(const QString &data)
{
    const QDateTime timestamp = QDateTime::currentDateTime();
    QRegularExpression reg("[$]([-+]?[0-9.]+)dBm([0-9.]+)(.)Vpp[$]");
    QRegularExpressionMatchIterator i = reg.globalMatch(data.simplified().replace(" ",""));

    if (i.isValid()) {
        while (i.hasNext()) {
            QRegularExpressionMatch match = i.next();
            bool ok;
            double dbm = match.captured(1).toDouble(&ok);
            if (ok) {
                double vpp_raw = match.captured(2).toDouble();
                if (match.captured(3) == 'u') vpp_raw /= 1000.0;
                emit measurementReady(timestamp, dbm, vpp_raw);
            } else {
                emit deviceError(tr("Could not parse dBm value from device."));
            }
        }
    } else {
        emit newLogMessage(QString("%1 [DEVICE] Non-measurement data: %2")
                               .arg(timestamp.toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                               .arg(data));
    }
}

void Rf8000Device::onSerialPortError(const QString &error)
{
    emit deviceError(error);
}
