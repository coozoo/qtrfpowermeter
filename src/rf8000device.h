#ifndef RF8000DEVICE_H
#define RF8000DEVICE_H

#include "abstractpmdevice.h"

class Rf8000Device : public AbstractPMDevice
{
    Q_OBJECT

public:
    explicit Rf8000Device(const PMDeviceProperties &props, QObject *parent = nullptr);
    ~Rf8000Device();

    void setFrequency(quint64 freqHz) override;
    void setOffset(double offsetDb) override;
    void connectDevice(const QString &portName) override;
    void disconnectDevice() override;
    void processData(const QString &data) override;

private slots:
    void onSerialPortNewData(const QString &data);
    void onSerialPortError(const QString &error);

private:
    SerialPortInterface *m_serialPort;
    quint64 m_currentFrequencyHz = 1000000;
    double m_currentOffsetDb = 0.0;
    bool m_isPositiveOffset = true;

    void sendCommand();
};

#endif // RF8000DEVICE_H
