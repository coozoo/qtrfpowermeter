#ifndef CONCEPTRFRPMDEVICE_H
#define CONCEPTRFRPMDEVICE_H

#include "abstractpmdevice.h"
#include <QTimer>
#include <QMap>
#include <QVector>

class SerialPortInterface;
class AbstractRpmLookupTable;

class ConceptRfRpmDevice : public AbstractPMDevice
{
    Q_OBJECT

public:
    explicit ConceptRfRpmDevice(const PMDeviceProperties &props, QObject *parent = nullptr);
    virtual ~ConceptRfRpmDevice();

    Q_INVOKABLE void connectDevice(const QString &portName) override;
    Q_INVOKABLE void disconnectDevice() override;
    Q_INVOKABLE void setFrequency(quint64 freqHz) override;
    Q_INVOKABLE void setOffset(double offsetDb) override;
    void processData(const QString &data) override;

    // Sampling: spec section 4 cmd 0x83. Same four rates on every RPM
    // model (10, 40, 640, 1280 Hz), index sent in payload[0].
    QList<int> supportedSamplingRatesHz() const override { return {10, 40, 640, 1280}; }
    int currentSamplingRateHz() const override;
    Q_INVOKABLE void setSamplingRateIndex(int index) override;

signals:
    void calibrationDownloadProgress(int percent);

private slots:
    void onPortOpened();
    void onPortClosed();
    void onSerialPortNewBinaryData(const QByteArray &data);
    void onSerialPortError(const QString &error);
    void onIdentificationTimeout();

private:
    enum DeviceState {
        Idle,
        Connecting,
        Identifying,           // sent 0x80, waiting for 0x00
        SettingSampling,       // sent 0x83, waiting for 0x03
        DownloadingCalibration,// sent 0x86 (0xFFFF then per-index), filling rows
        Ready                  // streaming 0x06 samples
    };

    void sendCommand(quint8 cmd, const QByteArray &payload = QByteArray());
    void handlePacket(quint8 cmd, const QByteArray &payload);
    void identifyDevice(quint16 deviceId, quint16 firmwareVersion, quint32 serialNumber);
    void beginSamplingConfig();
    void startCalibrationDownload();
    void requestSpecificCalibrationRow(int freqIndex);
    void finishInitialisationIfReady();
    double convertAdcToDbm(qint32 rawAdcValue);

    DeviceState m_state;
    SerialPortInterface *m_serialPort;
    QByteArray m_receiveBuffer;
    QTimer *m_identificationTimer;

    AbstractRpmLookupTable *m_lookupTable;

    quint16 m_firmwareVersion = 0;     // wire value; display as fw/100.0
    quint32 m_serialNumber = 0;
    int m_currentSamplingIndex = 0;    // mirrors what cmd 0x83 last sent

    quint64 m_currentFrequencyHz;
    double m_currentOffsetDb;

    static QMap<quint16, PMDeviceProperties> s_deviceSpecificProperties;
    static void initializePropertiesMap();

public:
    quint16 firmwareVersionRaw() const { return m_firmwareVersion; }
    double firmwareVersion() const { return m_firmwareVersion / 100.0; }
    quint32 serialNumber() const { return m_serialNumber; }

    // Wire-byte decoders, exposed as static so tests can verify endianness
    // independently of device state.
    struct IdentifyFields {
        quint16 deviceId;
        quint16 firmwareVersion;
        quint32 serialNumber;
        bool ok;
    };
    static IdentifyFields decodeIdentifyPayload(const QByteArray &payload);
    static qint32 decodeStreamingSamplePayload(const QByteArray &payload);
};

#endif // CONCEPTRFRPMDEVICE_H
