#ifndef PMDEVICEPROPERTIES_H
#define PMDEVICEPROPERTIES_H

#include <QString>
#include <QIcon>

struct PMDeviceProperties
{
    QString id; // Unique identifier, e.g., "rf8000"
    QString name; // User-friendly name, e.g., "RF Power Meter 8GHz"
    QString imagePath; // Path in resources, e.g., ":/images/rf8000.svg"
    QString alternativeNames;

    quint64 minFreqHz = 1000000;
    quint64 maxFreqHz = 8000000000;

    double minPowerDbm = -65.0;
    double maxPowerDbm = -5.0;

    bool hasOffset = true;
    qint32 baudRate = 9600;

    // properties for built-in attenuator
    bool hasInternalAttenuator = false;
    double internalAttMinDb = 0.0;
    double internalAttMaxDb = 0.0;
    double internalAttStepDb = 0.1;

    bool isEnabled = true;

    QList<QPair<quint16, quint16>> supportedVidPids;

    QIcon icon() const { return QIcon(imagePath); }
};

#endif // PMDEVICEPROPERTIES_H
