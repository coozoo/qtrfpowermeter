#ifndef ABSTRACTPMDEVICE_H
#define ABSTRACTPMDEVICE_H

#include <QObject>
#include "serialportinterface.h"
#include "pmdeviceproperties.h"

class AbstractPMDevice : public QObject
{
    Q_OBJECT

public:
    explicit AbstractPMDevice(const PMDeviceProperties &props, QObject *parent = nullptr)
        : QObject(parent), m_properties(props) {}
    virtual ~AbstractPMDevice() = default;

    const PMDeviceProperties& properties() const { return m_properties; }

    virtual void setFrequency(quint64 freqHz) = 0;
    virtual void setOffset(double offsetDb) = 0;
    virtual void setInternalAttenuation(double attDb) { Q_UNUSED(attDb); }
    virtual void connectDevice(const QString &portName) = 0;
    virtual void disconnectDevice() = 0;

    virtual void processData(const QString &data) = 0;

signals:
    void deviceConnected();
    void deviceDisconnected();
    void deviceError(const QString &error);

    void rawDataReceived(const QString &data);
    void measurementReady(double dbm, double vpp_raw);
    void newLogMessage(const QString &message);
    void internalAttenuationChanged(double attDb);

protected:
    PMDeviceProperties m_properties;
};

#endif // ABSTRACTPMDEVICE_H
