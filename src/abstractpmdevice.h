#ifndef ABSTRACTPMDEVICE_H
#define ABSTRACTPMDEVICE_H

#include <QObject>
#include "serialportinterface.h"
#include "pmdeviceproperties.h"
#include <QDateTime>

class AbstractPMDevice : public QObject
{
    Q_OBJECT

public:
    explicit AbstractPMDevice(const PMDeviceProperties &props, QObject *parent = nullptr)
        : QObject(parent), m_properties(props), m_loggingEnabled(true) {}
    virtual ~AbstractPMDevice() = default;

    const PMDeviceProperties& properties() const { return m_properties; }

    // --- Public API ---
    Q_INVOKABLE virtual void setFrequency(quint64 freqHz) = 0;
    Q_INVOKABLE virtual void setOffset(double offsetDb) = 0;
    Q_INVOKABLE virtual void setInternalAttenuation(double attDb) { Q_UNUSED(attDb); }
    Q_INVOKABLE virtual void connectDevice(const QString &portName) = 0;
    Q_INVOKABLE virtual void disconnectDevice() = 0;

    virtual void processData(const QString &data) = 0;

    // --- Logging Control ---
    Q_INVOKABLE void setLoggingEnabled(bool enabled) { m_loggingEnabled = enabled; }
    bool isLoggingEnabled() const { return m_loggingEnabled; }

    // --- Sampling rate ---
    // Devices that expose a programmable sample rate (currently
    // ConceptRfRpmDevice) override this. The returned list is the
    // user-visible rates in Hz; index 0 is the default applied at
    // connect.
    virtual QList<int> supportedSamplingRatesHz() const { return {}; }
    virtual int currentSamplingRateHz() const { return 0; }
    Q_INVOKABLE virtual void setSamplingRateIndex(int index) { Q_UNUSED(index); }

signals:
    void deviceConnected();
    void deviceDisconnected();
    void deviceError(const QString &error);

    void rawDataReceived(const QString &data);
    void measurementReady(QDateTime timestamp, double dbm, double vpp_raw);
    void newLogMessage(const QString &message);
    void internalAttenuationChanged(double attDb);
    
    void propertiesUpdated(const PMDeviceProperties &newProps);

    // Emitted by devices that report a model name / firmware version /
    // serial number at runtime (currently ConceptRfRpmDevice). Legacy
    // text-protocol devices do not emit it.
    void deviceIdentityChanged(const QString &modelName,
                               const QString &firmwareVersion,
                               const QString &serialNumber);

protected:
    PMDeviceProperties m_properties;
    bool m_loggingEnabled;
};

#endif // ABSTRACTPMDEVICE_H
