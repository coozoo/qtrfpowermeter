#include "rfpmv5device.h"
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>

RfpmV5Device::RfpmV5Device(const PMDeviceProperties &props, QObject *parent)
    : Rf8000Device(props, parent)
    , m_accumulatedDbm(0.0)
    , m_sampleCount(0)
    , m_timerIntervalMs(100) // 10 updates per second (10Hz)
    , m_skipCounter(0)
{
    m_sampleTimer = new QTimer(this);
    m_sampleTimer->setSingleShot(false);
    // for configurable via properties read props here
    // m_timerIntervalMs = props.updateInterval; 
    connect(m_sampleTimer, &QTimer::timeout, this, &RfpmV5Device::onSampleTimerTimeout);
}

void RfpmV5Device::connectDevice(const QString &portName)
{
    Rf8000Device::connectDevice(portName);
    
    if (m_serialPort->isPortOpen()) {
        m_accumulatedDbm = 0.0;
        m_sampleCount = 0;
        m_skipCounter = 0;
        m_lastRawPacket.clear();
        m_sampleTimer->start(m_timerIntervalMs);
        
        readSettings();
    }
}

void RfpmV5Device::disconnectDevice()
{
    m_sampleTimer->stop();
    Rf8000Device::disconnectDevice();
}

void RfpmV5Device::readSettings()
{
    if (m_serialPort->isPortOpen()) {
        emit newLogMessage(QString("%1 [DEVICE] Sending Read command...")
                               .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz")));
        m_serialPort->writeData("Read\r\n");
    }
}

void RfpmV5Device::sendBufferedCommand()
{
    if (!m_serialPort->isPortOpen()) return;

    int freqMhz = m_currentFrequencyHz / 1000000;
    if (freqMhz > 9999) freqMhz = 9999;

    QString sign = m_isPositiveOffset ? "+" : "-";
    
    QString offsetStr = QString::number(m_currentOffsetDb, 'f', 2);
    if (m_currentOffsetDb < 10.0) {
        offsetStr = "0" + offsetStr;
    }

    QString command = QString("A%1%2%3\r\n")
                          .arg(freqMhz, 4, 10, QChar('0'))
                          .arg(sign)
                          .arg(offsetStr);

    m_serialPort->writeData(command.toLatin1());
    emit newLogMessage(QString("%1 [DEVICE] Sent: %2")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                           .arg(command.trimmed()));
}

void RfpmV5Device::processData(const QString &data)
{
    m_buffer += data;

    // 1. CONFIG CHECK (R...A)
    if (m_buffer.contains('R')) {
        // Regex: R(Digits)(Sign)(Digits.Digits)A
        // e.g. R5800-00.5A
        static QRegularExpression configReg("R(\\d+)([-+])([\\d\\.]+)A");
        QRegularExpressionMatch configMatch = configReg.match(m_buffer);
        
        if (configMatch.hasMatch()) {
            QString fullMatch = configMatch.captured(0);
            emit newLogMessage(QString("%1 [DEVICE] Config Read: %2")
                                   .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                                   .arg(fullMatch));
            
            // Remove up to end of match
            m_buffer = m_buffer.mid(configMatch.capturedEnd());
        }
    }

    // 2. MEASUREMENT PARSING
    // Regex: (+/-)(3digits)(5digits)(unit)
    static QRegularExpression reg("([-+])(\\d{3})(\\d{5})([umw])");
    
    QRegularExpressionMatchIterator i = reg.globalMatch(m_buffer);
    int lastEnd = 0;

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        lastEnd = match.capturedEnd();
        
        // --- DECIMATION ---
        // ~3000 packets/sec.  process/log ~100/sec.
        // Skip 29 packets, process the 30th.
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
    // Timer runs at 10Hz (100ms).
    // With 3000 packets/sec and 1/30 decimation, we get ~100 samples/sec.
    // So m_sampleCount should be around 10 here.
    if (m_sampleCount > 0) {
        
        if (!m_lastRawPacket.isEmpty()) {
            emit newLogMessage(QString("%1 %2")
                                   .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                                   .arg(m_lastRawPacket));

            m_lastRawPacket.clear();
        }

        double avgDbm = m_accumulatedDbm / m_sampleCount;
        
        emit measurementReady(QDateTime::currentDateTime(), avgDbm, 0.0);

        // Reset accumulators
        m_accumulatedDbm = 0.0;
        m_sampleCount = 0;
    }
}
