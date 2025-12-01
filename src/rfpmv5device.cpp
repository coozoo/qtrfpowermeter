#include "rfpmv5device.h"
#include <QRegularExpression>
#include <QDateTime>
#include <QDebug>

RfpmV5Device::RfpmV5Device(const PMDeviceProperties &props, QObject *parent)
    : Rf8000Device(props, parent)
    , m_accumulatedDbm(0.0)
    , m_sampleCount(0)
    , m_timerIntervalMs(100) // 10 updates per second (10Hz)
{
    // Create the sampling timer
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
        m_sampleTimer->start(m_timerIntervalMs);
    }
}

void RfpmV5Device::disconnectDevice()
{
    m_sampleTimer->stop();
   
    Rf8000Device::disconnectDevice();
}

void RfpmV5Device::sendBufferedCommand()
{
    if (!m_serialPort->isPortOpen()) return;

    // V5 Format: A0433+30.00
    // Freq is 4 digits MHz
    int freqMhz = m_currentFrequencyHz / 1000000;
    if (freqMhz > 9999) freqMhz = 9999;

    // FIX: Construct sign manually. 
    // m_currentOffsetDb holds the absolute value (magnitude).
    // m_isPositiveOffset holds the sign state.
    QString sign = m_isPositiveOffset ? "+" : "-";

    QString command = QString("A%1%2%3\r\n")
                          .arg(freqMhz, 4, 10, QChar('0'))
                          .arg(sign)
                          .arg(m_currentOffsetDb, 0, 'f', 2);

    m_serialPort->writeData(command.toLatin1());
    emit newLogMessage(QString("%1 [DEVICE] Sent: %2")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz"))
                           .arg(command.trimmed()));
}

void RfpmV5Device::processData(const QString &data)
{
    // Append to the protected buffer from parent class
    m_buffer += data;

    // V5 Protocol Regex: 
    // Group 1: Sign (+/-)
    // Group 2: dBm * 10 (3 digits)
    // Group 3: Power * 100 (5 digits)
    // Group 4: Unit (u, m, w)
    static QRegularExpression reg("([-+])(\\d{3})(\\d{5})([umw])");
    
    QRegularExpressionMatchIterator i = reg.globalMatch(m_buffer);
    int lastEnd = 0;

    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        lastEnd = match.capturedEnd();

        double dbm = match.captured(2).toDouble() / 10.0;
        if (match.captured(1) == "-") {
            dbm = -dbm;
        }
        
        // Accumulate values instead of emitting immediately
        m_accumulatedDbm += dbm;
        m_sampleCount++;
    }

    if (lastEnd > 0) {
        m_buffer = m_buffer.mid(lastEnd);
    }

    // Safety buffer limit
    if (m_buffer.length() > 20000) {
        m_buffer.clear();
    }
}

void RfpmV5Device::onSampleTimerTimeout()
{
    if (m_sampleCount > 0) {
        // Calculate average dBm
        double avgDbm = m_accumulatedDbm / m_sampleCount;
        
        // Emit ONE update for this interval
        emit measurementReady(QDateTime::currentDateTime(), avgDbm, 0.0);
        
        // Reset accumulators
        m_accumulatedDbm = 0.0;
        m_sampleCount = 0;
    }
}
