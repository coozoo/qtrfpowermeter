#ifndef RF8000DEVICE_H
#define RF8000DEVICE_H

#include "abstractpmdevice.h"
#include <QTimer>

class Rf8000Device : public AbstractPMDevice
{
    Q_OBJECT

public:
    explicit Rf8000Device(const PMDeviceProperties &props, QObject *parent = nullptr);
    virtual ~Rf8000Device();

    Q_INVOKABLE void setFrequency(quint64 freqHz) override;
    Q_INVOKABLE void setOffset(double offsetDb) override;
    Q_INVOKABLE void connectDevice(const QString &portName) override;
    Q_INVOKABLE void disconnectDevice() override;
    Q_INVOKABLE void processData(const QString &data) override;

protected slots:
    void onSerialPortNewData(const QString &data);
    void onSerialPortError(const QString &error);
    virtual void sendBufferedCommand();

protected:
    SerialPortInterface *m_serialPort;
    quint64 m_currentFrequencyHz = 1000000;
    double m_currentOffsetDb = 0.0;
    bool m_isPositiveOffset = true;
    QTimer *m_commandTimer;
    QString m_buffer;

    void sendCommand();
};

#endif // RF8000DEVICE_H
