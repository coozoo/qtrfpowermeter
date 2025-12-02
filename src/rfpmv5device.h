#ifndef RFPMV5DEVICE_H
#define RFPMV5DEVICE_H

#include "rf8000device.h"

class RfpmV5Device : public Rf8000Device
{
    Q_OBJECT

public:
    explicit RfpmV5Device(const PMDeviceProperties &props, QObject *parent = nullptr);

    Q_INVOKABLE void connectDevice(const QString &portName) override;
    Q_INVOKABLE void disconnectDevice() override;
    void processData(const QString &data) override;

    Q_INVOKABLE void readSettings();

protected slots:
    void sendBufferedCommand() override;
    void onSampleTimerTimeout();

private:
    QTimer *m_sampleTimer;
    double m_accumulatedDbm;
    int m_sampleCount;
    int m_timerIntervalMs;
    int m_skipCounter;
    QString m_lastRawPacket;
};

#endif // RFPMV5DEVICE_H
