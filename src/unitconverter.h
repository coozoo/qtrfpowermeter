#ifndef UNITCONVERTER_H
#define UNITCONVERTER_H

#include <QString>
#include <QPair>
#include <QtMath>

class UnitConverter
{
public:
    // --- Power Conversions ---
    static double dBmToMilliwatts(double dBm);
    static double milliwattsToDBm(double milliwatts);
    static double milliwattsToVpp(double milliwatts, double impedance = 50.0);

    // --- Formatting ---
    static QPair<double, QString> formatPower(double milliwatts);
    static QPair<double, QString> formatVoltage(double millivolts);
};

#endif // UNITCONVERTER_H
