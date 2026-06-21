// Pure-function tests for UnitConverter. Locks down the dBm/mW round-trip,
// the V_pp derivation, and the metric-prefix boundaries used by the UI.

#include <QTest>
#include <QtMath>
#include "unitconverter.h"

namespace
{
constexpr double kEps = 1e-9;
}

class TestUnitConverter : public QObject
{
    Q_OBJECT

private slots:
    void dBmToMilliwatts_knownValues_data();
    void dBmToMilliwatts_knownValues();

    void milliwattsToDBm_knownValues_data();
    void milliwattsToDBm_knownValues();

    void milliwattsToDBm_zeroOrNegative_returnsNegInf();

    void milliwattsToVpp_oneMilliwatt_50Ohm();
    void milliwattsToVpp_negativeOrZeroImpedance_returnsZero();
    void milliwattsToVpp_negativePower_returnsZero();

    void formatPower_prefixBoundaries_data();
    void formatPower_prefixBoundaries();
    void formatPower_zeroAndNegative_collapseToZeroMW();

    void formatVoltage_prefixBoundaries_data();
    void formatVoltage_prefixBoundaries();
    void formatVoltage_zeroAndNegative_collapseToZeroM();
};

void TestUnitConverter::dBmToMilliwatts_knownValues_data()
{
    QTest::addColumn<double>("dbm");
    QTest::addColumn<double>("expectedMw");

    QTest::newRow("0 dBm = 1 mW")      << 0.0   << 1.0;
    QTest::newRow("10 dBm = 10 mW")    << 10.0  << 10.0;
    QTest::newRow("-10 dBm = 0.1 mW")  << -10.0 << 0.1;
    QTest::newRow("30 dBm = 1000 mW")  << 30.0  << 1000.0;
    QTest::newRow("-30 dBm = 0.001 mW")<< -30.0 << 0.001;
}

void TestUnitConverter::dBmToMilliwatts_knownValues()
{
    QFETCH(double, dbm);
    QFETCH(double, expectedMw);
    QVERIFY(qAbs(UnitConverter::dBmToMilliwatts(dbm) - expectedMw) < kEps * qMax(1.0, expectedMw));
}

void TestUnitConverter::milliwattsToDBm_knownValues_data()
{
    QTest::addColumn<double>("mw");
    QTest::addColumn<double>("expectedDbm");

    QTest::newRow("1 mW = 0 dBm")       << 1.0    << 0.0;
    QTest::newRow("10 mW = 10 dBm")     << 10.0   << 10.0;
    QTest::newRow("0.1 mW = -10 dBm")   << 0.1    << -10.0;
    QTest::newRow("1000 mW = 30 dBm")   << 1000.0 << 30.0;
}

void TestUnitConverter::milliwattsToDBm_knownValues()
{
    QFETCH(double, mw);
    QFETCH(double, expectedDbm);
    QVERIFY(qAbs(UnitConverter::milliwattsToDBm(mw) - expectedDbm) < 1e-9);
}

void TestUnitConverter::milliwattsToDBm_zeroOrNegative_returnsNegInf()
{
    QVERIFY(qIsInf(UnitConverter::milliwattsToDBm(0.0)));
    QVERIFY(UnitConverter::milliwattsToDBm(0.0) < 0);
    QVERIFY(qIsInf(UnitConverter::milliwattsToDBm(-1.0)));
    QVERIFY(UnitConverter::milliwattsToDBm(-1.0) < 0);
}

void TestUnitConverter::milliwattsToVpp_oneMilliwatt_50Ohm()
{
    // P = 1 mW, R = 50 Ohm
    // V_rms = sqrt(0.001 * 50) = sqrt(0.05) V
    // V_pp = 2*sqrt(2)*V_rms ~= 0.6324555 V = 632.4555 mV
    const double mvpp = UnitConverter::milliwattsToVpp(1.0);
    QVERIFY(qAbs(mvpp - 632.45553203) < 1e-6);
}

void TestUnitConverter::milliwattsToVpp_negativeOrZeroImpedance_returnsZero()
{
    QCOMPARE(UnitConverter::milliwattsToVpp(1.0, 0.0), 0.0);
    QCOMPARE(UnitConverter::milliwattsToVpp(1.0, -50.0), 0.0);
}

void TestUnitConverter::milliwattsToVpp_negativePower_returnsZero()
{
    QCOMPARE(UnitConverter::milliwattsToVpp(-1.0), 0.0);
}

void TestUnitConverter::formatPower_prefixBoundaries_data()
{
    QTest::addColumn<double>("mw");
    QTest::addColumn<double>("expectedValue");
    QTest::addColumn<QString>("expectedUnit");

    QTest::newRow("1 pW")  << 1e-9 << 1.0 << QStringLiteral("pW");
    QTest::newRow("1 nW")  << 1e-6 << 1.0 << QStringLiteral("nW");
    QTest::newRow("1 uW")  << 1e-3 << 1.0 << QStringLiteral("µW");
    QTest::newRow("1 mW")  << 1.0  << 1.0 << QStringLiteral("mW");
    QTest::newRow("1 W")   << 1e3  << 1.0 << QStringLiteral("W");
    QTest::newRow("1 kW")  << 1e6  << 1.0 << QStringLiteral("kW");
    QTest::newRow("1 MW")  << 1e9  << 1.0 << QStringLiteral("MW");
    QTest::newRow("1 GW")  << 1e12 << 1.0 << QStringLiteral("GW");
    QTest::newRow("999 mW just below W")
            << 999.0 << 999.0 << QStringLiteral("mW");
}

void TestUnitConverter::formatPower_prefixBoundaries()
{
    QFETCH(double, mw);
    QFETCH(double, expectedValue);
    QFETCH(QString, expectedUnit);

    const QPair<double, QString> r = UnitConverter::formatPower(mw);
    QVERIFY(qAbs(r.first - expectedValue) < 1e-6 * qMax(1.0, expectedValue));
    QCOMPARE(r.second, expectedUnit);
}

void TestUnitConverter::formatPower_zeroAndNegative_collapseToZeroMW()
{
    const QPair<double, QString> z = UnitConverter::formatPower(0.0);
    QCOMPARE(z.first, 0.0);
    QCOMPARE(z.second, QStringLiteral("mW"));

    const QPair<double, QString> n = UnitConverter::formatPower(-1.0);
    QCOMPARE(n.first, 0.0);
    QCOMPARE(n.second, QStringLiteral("mW"));
}

void TestUnitConverter::formatVoltage_prefixBoundaries_data()
{
    QTest::addColumn<double>("mvolts");
    QTest::addColumn<double>("expectedValue");
    QTest::addColumn<QString>("expectedPrefix");

    QTest::newRow("1 nV")  << 1e-6 << 1.0 << QStringLiteral("n");
    QTest::newRow("1 uV")  << 1e-3 << 1.0 << QStringLiteral("µ");
    QTest::newRow("1 mV")  << 1.0  << 1.0 << QStringLiteral("m");
    QTest::newRow("1 V")   << 1e3  << 1.0 << QStringLiteral("");
    QTest::newRow("1 kV")  << 1e6  << 1.0 << QStringLiteral("k");
    QTest::newRow("999 mV just below V")
            << 999.0 << 999.0 << QStringLiteral("m");
}

void TestUnitConverter::formatVoltage_prefixBoundaries()
{
    QFETCH(double, mvolts);
    QFETCH(double, expectedValue);
    QFETCH(QString, expectedPrefix);

    const QPair<double, QString> r = UnitConverter::formatVoltage(mvolts);
    QVERIFY(qAbs(r.first - expectedValue) < 1e-6 * qMax(1.0, expectedValue));
    QCOMPARE(r.second, expectedPrefix);
}

void TestUnitConverter::formatVoltage_zeroAndNegative_collapseToZeroM()
{
    QCOMPARE(UnitConverter::formatVoltage(0.0).first, 0.0);
    QCOMPARE(UnitConverter::formatVoltage(0.0).second, QStringLiteral("m"));
    QCOMPARE(UnitConverter::formatVoltage(-1.0).first, 0.0);
    QCOMPARE(UnitConverter::formatVoltage(-1.0).second, QStringLiteral("m"));
}

QTEST_APPLESS_MAIN(TestUnitConverter)
#include "test_unitconverter.moc"
