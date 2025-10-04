#ifndef CALIBRATIONPOINT_H
#define CALIBRATIONPOINT_H

#include <QObject>

struct CalibrationPoint
{
    double frequencyMHz = 0.0;
    double correctionDb = 0.0;
    bool isSet = false;
};


Q_DECLARE_METATYPE(CalibrationPoint)

#endif // CALIBRATIONPOINT_H
