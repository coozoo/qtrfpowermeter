#ifndef RF8000DEVICE_H
#define RF8000DEVICE_H

#include "abstractpmdevice.h"
#include <QTimer>

class Rf8000Device : public AbstractPMDevice
{
    Q_OBJECT

public:
    explicit Rf8000Device(const PMDeviceProperties &props, QObject *parent = nullptr);
    ~Rf8000Device();

    Q_INVOKABLE void setFrequency(quint64 freqHz) override;
    Q_INVOKABLE void setOffset(double offsetDb) override;
    Q_INVOKABLE void connectDevice(const QString &portName) override;
    Q_INVOKABLE void disconnectDevice() override;
    Q_INVOKABLE void processData(const QString &data) override;

private slots:
    void onSerialPortNewData(const QString &data);
    void onSerialPortError(const QString &error);
    void sendBufferedCommand();

private:
    SerialPortInterface *m_serialPort;
    quint64 m_currentFrequencyHz = 1000000;
    double m_currentOffsetDb = 0.0;
    bool m_isPositiveOffset = true;
    QTimer *m_commandTimer;s

    void sendCommand();
};

#endif // RF8000DEVICE_H
