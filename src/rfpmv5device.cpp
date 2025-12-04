#include "rfpmv5device.h"
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>

RfpmV5Device::RfpmV5Device(const PMDeviceProperties &props, QObject *parent)
    : AbstractPMDevice(props, parent)
    , m_accumulatedDbm(0.0)
    , m_sampleCount(0)
    , m_skipCounter(0)
    , m_timerIntervalMs(100)
{
    m_serialPort = new SerialPortInterface(this);
    connect(m_serialPort, &SerialPortInterface::serialPortNewData, this, &RfpmV5Device::onSerialPortNewData);
    connect(m_serialPort, &SerialPortInterface::serialPortErrorSignal, this, &RfpmV5Device::onSerialPortError);
    connect(m_serialPort, &SerialPortInterface::portOpened, this, &AbstractPMDevice::deviceConnected);
    connect(m_serialPort, &SerialPortInterface::portClosed, this, &AbstractPMDevice::deviceDisconnected);

    m_commandTimer = new QTimer(this);
    m_commandTimer->setSingleShot(true);
    m_commandTimer->setInterval(100);
    connect(m_commandTimer, &QTimer::timeout, this, &RfpmV5Device::sendBufferedCommand);

    m_sampleTimer = new QTimer(this);
    m_sampleTimer->setSingleShot(false);
    // for configurable via properties read props here
    // m_timerIntervalMs = props.updateInterval;
    connect(m_sampleTimer, &QTimer::timeout, this, &RfpmV5Device::onSampleTimerTimeout);

    m_readbackTimer = new QTimer(this);
    m_readbackTimer->setSingleShot(true);
    m_readbackTimer->setInterval(3000);
    connect(m_readbackTimer, &QTimer::timeout, this, &RfpmV5Device::readSettings);

    m_identificationTimer = new QTimer(this);
    m_identificationTimer->setSingleShot(true);
    m_identificationTimer->setInterval(m_readbackTimer->interval() + 5000);
    connect(m_identificationTimer, &QTimer::timeout, this, &RfpmV5Device::onIdentificationTimeout);
}

RfpmV5Device::~RfpmV5Device()
{
}

void RfpmV5Device::connectDevice(const QString &portName)
{
    m_serialPort->setportName(portName);
    m_serialPort->setbaudRate(m_properties.baudRate);
    m_serialPort->startPort();

    if (m_serialPort->isPortOpen()) {
        m_accumulatedDbm = 0.0;
        m_sampleCount = 0;
        m_skipCounter = 0;
        m_lastRawPacket.clear();
        m_buffer.clear();
        m_isIdentified = false;

        m_sampleTimer->start(m_timerIntervalMs);

        setSampleRate(1);
        m_readbackTimer->start();
        m_identificationTimer->start();
    }
}

void RfpmV5Device::disconnectDevice()
{
    m_commandTimer->stop();
    m_sampleTimer->stop();
    m_readbackTimer->stop();
    m_identificationTimer->stop();
    m_serialPort->stopPort();
}

void RfpmV5Device::setFrequency(quint64 freqHz)
{
    m_currentFrequencyHz = freqHz;
    if (!m_commandTimer->isActive()) {
        m_commandTimer->start();
    }
    m_readbackTimer->start();
}

void RfpmV5Device::setOffset(double offsetDb)
{
    m_currentOffsetDb = offsetDb;
    if (!m_commandTimer->isActive()) {
        m_commandTimer->start();
    }
    m_readbackTimer->start();
}

void RfpmV5Device::readSettings()
{
    if (m_serialPort->isPortOpen()) {
        emit newLogMessage(QString("%1 [DEVICE] Sending Read command...")
                               .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz")));
        m_serialPort->writeData(QString("Read\r\n").toLatin1());
    }
}

void RfpmV5Device::setSampleRate(int rate)
{
    if (!m_serialPort->isPortOpen()) return;
    if (rate < 1 || rate > 18) {
        emit deviceError(tr("Sample rate must be between 1 and 18."));
        return;
    }

    QString command = QString("K%1\r\n").arg(rate, 2, 10, QChar('0'));
    m_serialPort->writeData(command.toLatin1());
    emit newLogMessage(QString("%1 [DEVICE] Sent Sample Rate: %2")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                           .arg(command.trimmed()));
}

void RfpmV5Device::sendBufferedCommand()
{
    if (!m_serialPort->isPortOpen()) return;

    QString command = QString("A%1%2\r\n")
                          .arg(QString::number(m_currentFrequencyHz / 1000000).rightJustified(4, '0'))
                          .arg(((m_currentOffsetDb >= 0) ? "+" : "-") + QString::number(qAbs(m_currentOffsetDb), 'f', 1).rightJustified(4, '0'));

    m_serialPort->writeData(command.toLatin1());
    emit newLogMessage(QString("%1 [DEVICE] Sent: %2")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                           .arg(command.trimmed()));

    m_readbackTimer->start();
}

void RfpmV5Device::onSerialPortNewData(const QString &data)
{
    emit rawDataReceived(data);
    processData(data);
}

void RfpmV5Device::processData(const QString &data)
{
    m_buffer += data;

    if (m_buffer.contains('R')) {
        static QRegularExpression configReg("R(\\d+)([-+])([\\d\\.]+)A");
        QRegularExpressionMatch configMatch = configReg.match(m_buffer);

        if (configMatch.hasMatch()) {
            if (!m_isIdentified) {
                m_isIdentified = true;
                m_identificationTimer->stop();
                emit newLogMessage("[DEVICE] RF-PM V5 device identified successfully.");
            }
            emit newLogMessage(QString("%1 [DEVICE] Config Read: %2")
                                   .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                                   .arg(configMatch.captured(0)));
            m_buffer.remove(configMatch.capturedStart(), configMatch.capturedLength());
        }
    }

    static QRegularExpression reg("([-+])(\\d{3})(\\d{5})([umw])");
    QRegularExpressionMatchIterator i = reg.globalMatch(m_buffer);
    int lastEnd = 0;

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        lastEnd = match.capturedEnd();
        m_skipCounter++;
        if (m_skipCounter >= 30) {
            m_skipCounter = 0;
            m_lastRawPacket = match.captured(0);

            double dbm = match.captured(2).toDouble() / 10.0;
            if (match.captured(1) == "-") {
                dbm = -dbm;
            }

            m_accumulatedDbm += dbm;
            m_sampleCount++;
        }
    }

    if (lastEnd > 0) {
        m_buffer = m_buffer.mid(lastEnd);
    }

    if (m_buffer.length() > 20000) {
        m_buffer.clear();
    }
}

void RfpmV5Device::onSampleTimerTimeout()
{
    if (m_sampleCount > 0) {
        if (!m_lastRawPacket.isEmpty()) {
            emit newLogMessage(QString("%1 %2")
                                   .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                                   .arg(m_lastRawPacket));
            m_lastRawPacket.clear();
        }
        double avgDbm = m_accumulatedDbm / m_sampleCount;
        emit measurementReady(QDateTime::currentDateTime(), avgDbm, 0.0);
        m_accumulatedDbm = 0.0;
        m_sampleCount = 0;
    }
}

void RfpmV5Device::onSerialPortError(const QString &error)
{
    emit deviceError(error);
}

void RfpmV5Device::onIdentificationTimeout()
{
    if (!m_isIdentified) {
        emit deviceError(tr("Device did not respond or is not a compatible RF-PM V5 device."));
        disconnectDevice();
    }
}
