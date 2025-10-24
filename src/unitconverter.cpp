#include "unitconverter.h"

double UnitConverter::dBmToMilliwatts(double dBm)
{
    return qPow(10, dBm / 10.0);
}

double UnitConverter::milliwattsToDBm(double milliwatts)
{
    if (milliwatts <= 0)
    {
        return -qInf();
    }
    return 10 * qLn(milliwatts) / qLn(10);
}

double UnitConverter::milliwattsToVpp(double milliwatts, double impedance)
{
    if (milliwatts < 0 || impedance <= 0) {
        return 0.0;
    }
    // V_rms = sqrt(P_W * R) = sqrt((milliwatts/1000) * impedance)
    // V_pp = 2 * sqrt(2) * V_rms
    // Vpp_mV = V_pp * 1000
    double v_rms = qSqrt((milliwatts / 1000.0) * impedance);
    return 2.0 * M_SQRT2 * v_rms * 1000.0;
}


QPair<double, QString> UnitConverter::formatPower(double milliwatts)
{
    // 1 GW = 1e12 mW
    if (milliwatts >= 1e12)
    {
        return qMakePair(milliwatts / 1e12, "GW");
    }
    // 1 MW = 1e9 mW
    else if (milliwatts >= 1e9)
    {
        return qMakePair(milliwatts / 1e9, "MW");
    }
    // 1 kW = 1e6 mW
    else if (milliwatts >= 1e6)
    {
        return qMakePair(milliwatts / 1e6, "kW");
    }
    // 1 W = 1e3 mW
    else if (milliwatts >= 1e3)
    {
        return qMakePair(milliwatts / 1e3, "W");
    }
    // 1 mW
    else if (milliwatts >= 1.0)
    {
        return qMakePair(milliwatts, "mW");
    }
    // 1 µW = 1e-3 mW
    else if (milliwatts >= 1e-3)
    {
        return qMakePair(milliwatts * 1e3, "µW");
    }
    // 1 nW = 1e-6 mW
    else if (milliwatts >= 1e-6)
    {
        return qMakePair(milliwatts * 1e6, "nW");
    }
    // 1 pW = 1e-9 mW
    else if (milliwatts > 0)
    {
        return qMakePair(milliwatts * 1e9, "pW");
    }
    // Default to 0 mW
    else
    {
        return qMakePair(0.0, "mW");
    }
}

QPair<double, QString> UnitConverter::formatVoltage(double millivolts)
{
    // Base unit is mV. Suffix "Vpp" add in the UI.
    // Returns the value and the metric prefix (k, "", m, µ, n)
    if (millivolts >= 1e6) // 1000V = 1e6 mV
    {
        return qMakePair(millivolts / 1e6, "k"); // Kilovolts
    }
    else if (millivolts >= 1e3) // 1V = 1e3 mV
    {
        return qMakePair(millivolts / 1e3, ""); // Volts (no prefix)
    }
    else if (millivolts >= 1.0)
    {
        return qMakePair(millivolts, "m"); // Millivolts
    }
    else if (millivolts >= 1e-3)
    {
        return qMakePair(millivolts * 1e3, "µ"); // Microvolts
    }
    else if (millivolts > 0)
    {
        return qMakePair(millivolts * 1e6, "n"); // Nanovolts
    }
    else
    {
        return qMakePair(0.0, "m"); // Default to millivolts
    }
}
