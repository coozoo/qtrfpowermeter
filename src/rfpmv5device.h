#ifndef RFPMV5DEVICE_H
#define RFPMV5DEVICE_H

#include "abstractpmdevice.h"
#include <QTimer>

class RfpmV5Device : public AbstractPMDevice
{
    Q_OBJECT

public:
    explicit RfpmV5Device(const PMDeviceProperties &props, QObject *parent = nullptr);
    virtual ~RfpmV5Device();

    Q_INVOKABLE void connectDevice(const QString &portName) override;
    Q_INVOKABLE void disconnectDevice() override;
    Q_INVOKABLE void setFrequency(quint64 freqHz) override;
    Q_INVOKABLE void setOffset(double offsetDb) override;
    void processData(const QString &data) override;

    Q_INVOKABLE void readSettings();
    Q_INVOKABLE void setSampleRate(int rate);

private slots:
    void onSerialPortNewData(const QString &data);
    void onSerialPortError(const QString &error);
    void sendBufferedCommand();
    void onSampleTimerTimeout();
    void onIdentificationTimeout();

private:
    SerialPortInterface *m_serialPort;
    QTimer *m_commandTimer;
    QTimer *m_sampleTimer;
    QTimer *m_readbackTimer;
    QTimer *m_identificationTimer;

    QString m_buffer;
    
    // Measurement state
    double m_accumulatedDbm;
    int m_sampleCount;
    int m_skipCounter;
    int m_timerIntervalMs;
    QString m_lastRawPacket;

    // Device state
    quint64 m_currentFrequencyHz;
    double m_currentOffsetDb;
    bool m_isIdentified = false;
};

#endif // RFPMV5DEVICE_H
