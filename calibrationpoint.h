#ifndef CALIBRATIONPOINT_H
#define CALIBRATIONPOINT_H

#include <QObject>

// A simple struct to hold the data for one calibration point.
struct CalibrationPoint
{
    double frequencyMHz = 0.0;
    double correctionDb = 0.0;
    bool isSet = false; // Flag to track if this point has been calibrated
};

// Register the struct with the Qt meta-object system to allow its use in signals/slots
Q_DECLARE_METATYPE(CalibrationPoint)

#endif // CALIBRATIONPOINT_H
