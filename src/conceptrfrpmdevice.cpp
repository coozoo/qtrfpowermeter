#include "conceptrfrpmdevice.h"
#include "serialportinterface.h"
#include "conceptrfrpmlookuptables.h"
#include <QDateTime>
#include <QDataStream>
#include <QDebug>

QMap<quint16, PMDeviceProperties> ConceptRfRpmDevice::s_deviceSpecificProperties;

ConceptRfRpmDevice::ConceptRfRpmDevice(const PMDeviceProperties &props, QObject *parent)
    : AbstractPMDevice(props, parent),
    m_state(Idle),
    m_lookupTable(nullptr),
    m_currentFrequencyHz(1000000000),
    m_currentOffsetDb(0.0)
{
    m_serialPort = new SerialPortInterface(this);
    m_identificationTimer = new QTimer(this);

    connect(m_serialPort, &SerialPortInterface::portOpened, this, &ConceptRfRpmDevice::onPortOpened);
    connect(m_serialPort, &SerialPortInterface::portClosed, this, &ConceptRfRpmDevice::onPortClosed);
    connect(m_serialPort, &SerialPortInterface::serialPortErrorSignal, this, &ConceptRfRpmDevice::onSerialPortError);
    connect(m_serialPort, &SerialPortInterface::serialPortNewBinaryData, this, &ConceptRfRpmDevice::onSerialPortNewBinaryData);

    m_identificationTimer->setSingleShot(true);
    m_identificationTimer->setInterval(3000);
    connect(m_identificationTimer, &QTimer::timeout, this, &ConceptRfRpmDevice::onIdentificationTimeout);

    if (s_deviceSpecificProperties.isEmpty()) {
        initializePropertiesMap();
    }
}

ConceptRfRpmDevice::~ConceptRfRpmDevice()
{
    delete m_lookupTable;
}

void ConceptRfRpmDevice::connectDevice(const QString &portName)
{
    if (m_state != Idle) return;
    m_state = Connecting;
    m_serialPort->setportName(portName);
    m_serialPort->setbaudRate(m_properties.baudRate);
    m_serialPort->startPort();
}

void ConceptRfRpmDevice::disconnectDevice()
{
    m_serialPort->stopPort();
}

void ConceptRfRpmDevice::onPortOpened()
{
    m_state = Identifying;
    m_receiveBuffer.clear();
    if (isLoggingEnabled()) emit newLogMessage("[DRIVER] Port opened. Requesting device info...");
    sendCommand(128); // Request "Get Device Info"
    m_identificationTimer->start();
}

void ConceptRfRpmDevice::onPortClosed()
{
    m_identificationTimer->stop();
    m_state = Idle;
    delete m_lookupTable;
    m_lookupTable = nullptr;
    emit deviceDisconnected();
}

void ConceptRfRpmDevice::setFrequency(quint64 freqHz) { m_currentFrequencyHz = freqHz; }
void ConceptRfRpmDevice::setOffset(double offsetDb) { m_currentOffsetDb = offsetDb; }

void ConceptRfRpmDevice::processData(const QString &data)
{
    // This function is required by the abstract base class but is intentionally
    // left empty. All data processing for this device happens in onSerialPortNewBinaryData.
    Q_UNUSED(data);
}

void ConceptRfRpmDevice::onSerialPortNewBinaryData(const QByteArray &data)
{
    m_receiveBuffer.append(data);

    while (m_receiveBuffer.size() >= 6) {
        if (static_cast<quint8>(m_receiveBuffer.at(0)) != 0x55 || static_cast<quint8>(m_receiveBuffer.at(1)) != 0xAA) {
            m_receiveBuffer.remove(0, 1);
            continue;
        }

        quint8 data_len = static_cast<quint8>(m_receiveBuffer.at(3));
        if (data_len != static_cast<quint8>(~m_receiveBuffer.at(4))) {
            m_receiveBuffer.remove(0, 1);
            continue;
        }

        int packet_len = 6 + data_len;
        if (m_receiveBuffer.size() < packet_len) {
            return;
        }

        QByteArray packet = m_receiveBuffer.left(packet_len);
        m_receiveBuffer.remove(0, packet_len);

        quint8 checksum = 0;
        for (int i = 2; i < packet_len - 1; ++i) { checksum += static_cast<quint8>(packet.at(i)); }

        if (checksum == static_cast<quint8>(packet.at(packet_len - 1))) {
            handlePacket(static_cast<quint8>(packet.at(2)), packet.mid(5, data_len));
        } else if (isLoggingEnabled()) {
            emit newLogMessage(QString("Checksum mismatch!"));
        }
    }
}

void ConceptRfRpmDevice::handlePacket(quint8 cmd, const QByteArray &payload)
{
    // NOTE: don't stop the watchdog timer unconditionally here. The
    // device starts streaming 0x06 samples the moment the port opens
    // (no handshake required from the firmware's side), and an
    // incompatible binary device on the same VID:PID could also be
    // emitting 0x55 0xAA framed messages. We only want to consider the
    // handshake "alive" when we get the response that *advances the
    // current state* -- otherwise the timeout would never fire even
    // though we're stuck.

    switch (cmd) {
    case 0: // Identify response: u16 device_id BE, u16 firmware_ver BE, u32 serial BE
        if (m_state == Identifying) {
            const IdentifyFields f = decodeIdentifyPayload(payload);
            if (f.ok) {
                m_identificationTimer->stop();
                identifyDevice(f.deviceId, f.firmwareVersion, f.serialNumber);
            }
        }
        break;

    case 3: // Sampling-rate ack: u8 sampling_index_applied
        if (payload.size() >= 1) {
            m_currentSamplingIndex = static_cast<quint8>(payload.at(0));
            if (m_state == SettingSampling) {
                m_identificationTimer->stop();
                if (isLoggingEnabled()) {
                    emit newLogMessage(QString("[DRIVER] Sampling configured (idx=%1). "
                                               "Starting calibration download.")
                                           .arg(m_currentSamplingIndex));
                }
                startCalibrationDownload();
            } else if (isLoggingEnabled()) {
                emit newLogMessage(QString("[DRIVER] Sampling changed (idx=%1).")
                                       .arg(m_currentSamplingIndex));
            }
        }
        break;

    case 5: // Calibration row: u16 freq_index BE + N x float32 LE
        if (m_state == DownloadingCalibration && payload.size() >= 2 && m_lookupTable) {
            m_identificationTimer->stop();
            const quint8 *p = reinterpret_cast<const quint8*>(payload.constData());
            quint16 freqId = (static_cast<quint16>(p[0]) << 8) | p[1];

            QByteArray voltageData = payload.mid(2);
            QVector<float> voltages;
            QDataStream stream(voltageData);
            stream.setByteOrder(QDataStream::LittleEndian);
            stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
            while (!stream.atEnd()) { float v; stream >> v; voltages.append(v); }

            m_lookupTable->setVoltageRow(freqId, voltages);

            const int total = m_lookupTable->getFreqTableSize();
            int filled = 0;
            for (int i = 0; i < total; ++i) if (m_lookupTable->isRowFilled(i)) ++filled;
            emit calibrationDownloadProgress((filled * 100) / qMax(1, total));

            finishInitialisationIfReady();
        }
        break;

    case 6: // Streaming power sample: s32 raw BE (signed 24-bit-in-32 ADC)
        if (m_state == Ready && payload.size() == 4) {
            qint32 raw_adc = decodeStreamingSamplePayload(payload);
            double dbm = convertAdcToDbm(raw_adc);
            if (dbm < 99999.0) { dbm += m_currentOffsetDb; }
            emit measurementReady(QDateTime::currentDateTime(), dbm, 0.0);
        }
        break;
    }
}

ConceptRfRpmDevice::IdentifyFields
ConceptRfRpmDevice::decodeIdentifyPayload(const QByteArray &payload)
{
    IdentifyFields f { 0, 0, 0, false };
    if (payload.size() < 8) return f;
    const quint8 *p = reinterpret_cast<const quint8*>(payload.constData());
    f.deviceId        = (static_cast<quint16>(p[0]) << 8) | p[1];
    f.firmwareVersion = (static_cast<quint16>(p[2]) << 8) | p[3];
    f.serialNumber    = (static_cast<quint32>(p[4]) << 24)
                      | (static_cast<quint32>(p[5]) << 16)
                      | (static_cast<quint32>(p[6]) << 8)
                      |  static_cast<quint32>(p[7]);
    f.ok = true;
    return f;
}

qint32 ConceptRfRpmDevice::decodeStreamingSamplePayload(const QByteArray &payload)
{
    if (payload.size() != 4) return 0;
    const quint8 *p = reinterpret_cast<const quint8*>(payload.constData());
    return static_cast<qint32>(
          (static_cast<quint32>(p[0]) << 24)
        | (static_cast<quint32>(p[1]) << 16)
        | (static_cast<quint32>(p[2]) << 8)
        |  static_cast<quint32>(p[3]));
}

void ConceptRfRpmDevice::identifyDevice(quint16 deviceId, quint16 firmwareVersion, quint32 serialNumber)
{
    if (!s_deviceSpecificProperties.contains(deviceId)) {
        emit deviceError(tr("Unsupported device ID: %1").arg(deviceId));
        disconnectDevice();
        return;
    }

    m_firmwareVersion = firmwareVersion;
    m_serialNumber    = serialNumber;

    // Overlay the model-specific fields (name, frequency range, power
    // range, baud, hasOffset) onto the factory-supplied properties so
    // that VID:PID, imagePath, alternativeNames and other transport-
    // level fields survive identification.
    const PMDeviceProperties overlay = s_deviceSpecificProperties.value(deviceId);
    m_properties.name         = overlay.name;
    m_properties.minFreqHz    = overlay.minFreqHz;
    m_properties.maxFreqHz    = overlay.maxFreqHz;
    m_properties.minPowerDbm  = overlay.minPowerDbm;
    m_properties.maxPowerDbm  = overlay.maxPowerDbm;
    m_properties.hasOffset    = overlay.hasOffset;
    m_properties.baudRate     = overlay.baudRate;

    delete m_lookupTable;
    m_lookupTable = nullptr;
    if (deviceId == 101 || deviceId == 104) { m_lookupTable = new Rpm20gsLookupTable(); }
    else if (deviceId == 102 || deviceId == 105) { m_lookupTable = new Rpm3gsLookupTable(); }
    else if (deviceId == 103 || deviceId == 106) { m_lookupTable = new Rpm9gLookupTable(); }
    else if (deviceId == 107) { m_lookupTable = new Rpm6ghLookupTable(); }

    if (m_lookupTable) {
        m_lookupTable->initializeTables();
        if (isLoggingEnabled()) {
            emit newLogMessage(QString("[DRIVER] Identified %1, fw %2, S/N %3.")
                                   .arg(m_properties.name)
                                   .arg(firmwareVersion / 100.0, 0, 'f', 2)
                                   .arg(serialNumber));
        }
        emit propertiesUpdated(m_properties);
        emit deviceIdentityChanged(
            m_properties.name,
            QString::number(firmwareVersion / 100.0, 'f', 2),
            QString::number(serialNumber));
        beginSamplingConfig();
    } else {
        emit deviceError(tr("Failed to create lookup table for device ID %1").arg(deviceId));
        disconnectDevice();
    }
}

void ConceptRfRpmDevice::beginSamplingConfig()
{
    // Per protocol section 8 (init handshake), cmd 0x83 with sampling
    // index 0 sits between identify and the calibration read. Some
    // firmware versions refuse to enter streaming without it.
    m_state = SettingSampling;
    m_currentSamplingIndex = 0;
    QByteArray payload;
    payload.append(char(m_currentSamplingIndex));
    sendCommand(0x83, payload);
    m_identificationTimer->start();
}

int ConceptRfRpmDevice::currentSamplingRateHz() const
{
    const QList<int> rates = supportedSamplingRatesHz();
    if (m_currentSamplingIndex >= 0 && m_currentSamplingIndex < rates.size()) {
        return rates.at(m_currentSamplingIndex);
    }
    return 0;
}

void ConceptRfRpmDevice::setSamplingRateIndex(int index)
{
    const QList<int> rates = supportedSamplingRatesHz();
    if (index < 0 || index >= rates.size()) return;
    m_currentSamplingIndex = index;
    QByteArray payload;
    payload.append(char(index));
    sendCommand(0x83, payload);
    if (isLoggingEnabled()) {
        emit newLogMessage(QString("[DRIVER] Sent sampling index %1 (%2 Hz)")
                               .arg(index).arg(rates.at(index)));
    }
}

void ConceptRfRpmDevice::startCalibrationDownload()
{
    if (!m_lookupTable) return;
    m_state = DownloadingCalibration;

    // Kick the firmware into "send all calibration rows" mode with the
    // 0xFFFF sentinel (spec section 4, cmd 0x86). Rows arrive
    // asynchronously; we then top up any unfilled indices one at a time.
    QByteArray ffff;
    ffff.append(char(0xFF));
    ffff.append(char(0xFF));
    sendCommand(0x86, ffff);

    // Immediately also request row 0 so progress moves even on firmware
    // versions that don't honour the 0xFFFF bulk dump.
    requestSpecificCalibrationRow(0);
}

void ConceptRfRpmDevice::requestSpecificCalibrationRow(int freqIndex)
{
    if (m_state != DownloadingCalibration) return;
    QByteArray payload;
    payload.append(static_cast<char>((freqIndex >> 8) & 0xFF));
    payload.append(static_cast<char>( freqIndex       & 0xFF));
    sendCommand(0x86, payload);
    m_identificationTimer->start(); // watchdog for the response
}

void ConceptRfRpmDevice::finishInitialisationIfReady()
{
    if (!m_lookupTable) return;
    if (m_lookupTable->allRowsFilled()) {
        m_state = Ready;
        m_identificationTimer->stop();   // handshake done, no more watchdog
        emit deviceConnected();
        if (isLoggingEnabled()) {
            emit newLogMessage("[DRIVER] Calibration download complete. Device is ready.");
        }
        return;
    }
    // Top up the next missing row. Spec section 8 state-3 search.
    const int next = m_lookupTable->firstUnfilledRow();
    if (next >= 0) requestSpecificCalibrationRow(next);
}

void ConceptRfRpmDevice::onIdentificationTimeout()
{
    // Ready means the handshake already completed; a stray late timer
    // tick should be a no-op (the timer is single-shot per state, but
    // belt-and-braces against future restarts).
    if (m_state == Ready || m_state == Idle) return;

    // Short, status-line-friendly per-state message. Matches the spirit
    // of RfpmV5Device::onIdentificationTimeout but stays one sentence.
    QString reason;
    switch (m_state) {
    case Identifying:
        reason = tr("No identify response \xe2\x80\x94 not a compatible Concept RF RPM.");
        break;
    case SettingSampling:
        reason = tr("Sampling config not acknowledged \xe2\x80\x94 reconnect.");
        break;
    case DownloadingCalibration:
        reason = tr("Calibration download stalled \xe2\x80\x94 reconnect.");
        break;
    default:
        reason = tr("Device failed to respond in time.");
        break;
    }
    emit deviceError(reason);
    disconnectDevice();
}

double ConceptRfRpmDevice::convertAdcToDbm(qint32 rawAdcValue)
{
    if (m_state != Ready || !m_lookupTable) return -999.9;
    return m_lookupTable->convert(rawAdcValue, m_currentFrequencyHz);
}

void ConceptRfRpmDevice::sendCommand(quint8 cmd, const QByteArray &payload)
{
    if (!m_serialPort->isPortOpen()) return;
    QByteArray packet;
    packet.append(QByteArray::fromHex("55AA"));
    packet.append(cmd);
    quint8 len = payload.size();
    packet.append(len);
    packet.append(~len);
    packet.append(payload);
    quint8 checksum = 0;
    for (int i = 2; i < packet.size(); ++i) { checksum += (quint8)packet.at(i); }
    packet.append(checksum);
    m_serialPort->writeData(packet);
}

void ConceptRfRpmDevice::onSerialPortError(const QString &error) { emit deviceError(error); }

void ConceptRfRpmDevice::initializePropertiesMap()
{
    s_deviceSpecificProperties.clear();
    PMDeviceProperties props;
    props.baudRate = 460800; props.hasOffset = true;

    props.name = "Concept RF RPM-20GS";
    props.minFreqHz = 10000000; props.maxFreqHz = 20000000000ULL;
    props.minPowerDbm = -40; props.maxPowerDbm = 10;
    s_deviceSpecificProperties.insert(101, props);
    s_deviceSpecificProperties.insert(104, props);

    props.name = "Concept RF RPM-3GS";
    props.minFreqHz = 50; props.maxFreqHz = 3000000000;
    props.minPowerDbm = -50; props.maxPowerDbm = 10;
    s_deviceSpecificProperties.insert(102, props);
    s_deviceSpecificProperties.insert(105, props);

    props.name = "Concept RF RPM-9G";
    props.minFreqHz = 10000000; props.maxFreqHz = 9000000000;
    props.minPowerDbm = -40; props.maxPowerDbm = 10;
    s_deviceSpecificProperties.insert(103, props);
    s_deviceSpecificProperties.insert(106, props);

    props.name = "Concept RF RPM-6GH";
    props.minFreqHz = 10000000; props.maxFreqHz = 6000000000;
    props.minPowerDbm = -80; props.maxPowerDbm = 20;
    s_deviceSpecificProperties.insert(107, props);
}
