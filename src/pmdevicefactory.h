#ifndef PMDEVICEFACTORY_H
#define PMDEVICEFACTORY_H

#include <QObject>
#include <QMap>
#include "pmdeviceproperties.h"

class AbstractPMDevice;

class PMDeviceFactory : public QObject
{
    Q_OBJECT

public:
    explicit PMDeviceFactory(QObject *parent = nullptr);
    ~PMDeviceFactory();

    AbstractPMDevice* createDevice(const QString &deviceId, QObject *parent = nullptr);
    QList<PMDeviceProperties> availableDevices() const;
    PMDeviceProperties propertiesForDevice(const QString &deviceId) const;

private:
    void registerDevices();
    QMap<QString, PMDeviceProperties> m_deviceRegistry;
};

#endif // PMDEVICEFACTORY_H
