/* this is just junkie prototype
 * I don't have such powermeter
 * and I don't have real example of output
 * it is made by description on ali
 * so for sure it shouldn't work
 */

#ifndef RFPMV7DEVICE_H
#define RFPMV7DEVICE_H

#include "abstractpmdevice.h"
#include "unitconverter.h"

class RfpmV7Device : public AbstractPMDevice
{
    Q_OBJECT

public:
    explicit RfpmV7Device(const PMDeviceProperties &props, QObject *parent = nullptr);
    ~RfpmV7Device();

    void setFrequency(quint64 freqHz) override;
    void setOffset(double offsetDb) override;
    void setInternalAttenuation(double attDb) override;
    void connectDevice(const QString &portName) override;
    void disconnectDevice() override;
    void processData(const QString &data) override;

private slots:
    void onSerialPortNewData(const QString &data);
    void onSerialPortError(const QString &error);

private:
    void sendWriteCommand();

    SerialPortInterface *m_serialPort;

    quint64 m_currentFrequencyHz = 1000000000;
    double m_currentOffsetDb = 0.0;
    double m_internalAttDb = 0.0;

    double m_lastDbm = -999.0;
    bool m_waitingForPower = false;
};

#endif // RFPMV7DEVICE_H
