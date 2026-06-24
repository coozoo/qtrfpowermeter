// Behaviour tests for CalibrationModel — focuses on the two non-Qt-model
// methods: generateFrequencies(start,end,step) and the correction lookup
// getCorrection(freqMHz), which has 0/1/2/>=3-point fallbacks around the
// cubic spline.

#include <QTest>
#include <QtMath>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include "calibrationmodel.h"
#include "calibrationpoint.h"

class TestCalibrationModel : public QObject
{
    Q_OBJECT

private slots:
    void generate_emptyOnInvalidStep();
    void generate_emptyOnInvertedRange();
    void generate_keepsExactStartAndEnd();
    void generate_intermediatePointsSnapToStep();

    void getCorrection_emptyModel_returnsZero();
    void getCorrection_singleSetPoint_returnsThatValue();
    void getCorrection_twoSetPoints_linearInterpolates();
    void getCorrection_twoSetPoints_clampsOutsideRange();
    void getCorrection_threeOrMoreSetPoints_passesThroughKnots();

    void mode_defaultIsSimple();
    void modeAwareCorrection_disabledReturnsZero();
    void modeAwareCorrection_simpleIgnoresMeasured();

    void advanced_powerAxisGen_clampsStepFloor();
    void advanced_setCell_storesAndInterpolates();
    void advanced_bilinearInterpolation_atCellExact();
    void advanced_bilinearInterpolation_betweenCells();
    void advanced_unfilledCell_fallsBackToNearestInRow();
    void advanced_emptyTableReturnsZero();

    void json_roundtripPreservesBothStores();
    void json_legacyProfileMigratesToSimple();
    void modeSwitch_preservesBothStores();
};

namespace
{
QVector<CalibrationPoint> withCorrection(QVector<CalibrationPoint> pts, int idx, double db)
{
    pts[idx].correctionDb = db;
    pts[idx].isSet = true;
    return pts;
}
}

void TestCalibrationModel::generate_emptyOnInvalidStep()
{
    CalibrationModel m;
    m.generateFrequencies(100, 200, 0);
    QCOMPARE(m.rowCount(), 0);

    m.generateFrequencies(100, 200, -5);
    QCOMPARE(m.rowCount(), 0);
}

void TestCalibrationModel::generate_emptyOnInvertedRange()
{
    CalibrationModel m;
    m.generateFrequencies(500, 100, 50);
    QCOMPARE(m.rowCount(), 0);

    m.generateFrequencies(100, 100, 50);
    QCOMPARE(m.rowCount(), 0);
}

void TestCalibrationModel::generate_keepsExactStartAndEnd()
{
    CalibrationModel m;
    m.generateFrequencies(13.0, 87.0, 10.0);
    const auto &pts = m.getPoints();
    QVERIFY(pts.size() >= 2);
    QCOMPARE(pts.first().frequencyMHz, 13.0);
    QCOMPARE(pts.last().frequencyMHz, 87.0);
}

void TestCalibrationModel::generate_intermediatePointsSnapToStep()
{
    CalibrationModel m;
    m.generateFrequencies(13.0, 47.0, 10.0);
    const auto &pts = m.getPoints();
    // Start=13, then ceil(13/10)*10=20 -> 20,30,40, then end=47
    QCOMPARE(pts.size(), 5);
    QCOMPARE(pts.at(0).frequencyMHz, 13.0);
    QCOMPARE(pts.at(1).frequencyMHz, 20.0);
    QCOMPARE(pts.at(2).frequencyMHz, 30.0);
    QCOMPARE(pts.at(3).frequencyMHz, 40.0);
    QCOMPARE(pts.at(4).frequencyMHz, 47.0);

    for (const auto &p : pts) {
        QVERIFY(!p.isSet);
        QCOMPARE(p.correctionDb, 0.0);
    }
}

void TestCalibrationModel::getCorrection_emptyModel_returnsZero()
{
    CalibrationModel m;
    QCOMPARE(m.getCorrection(100.0), 0.0);
}

void TestCalibrationModel::getCorrection_singleSetPoint_returnsThatValue()
{
    CalibrationModel m;
    QVector<CalibrationPoint> pts;
    pts.push_back({100.0, 0.0, false});
    pts.push_back({200.0, 0.0, false});
    pts = withCorrection(pts, 0, 1.5);
    m.setPoints(pts);

    QCOMPARE(m.getCorrection(50.0),  1.5);
    QCOMPARE(m.getCorrection(100.0), 1.5);
    QCOMPARE(m.getCorrection(300.0), 1.5);
}

void TestCalibrationModel::getCorrection_twoSetPoints_linearInterpolates()
{
    CalibrationModel m;
    QVector<CalibrationPoint> pts;
    pts.push_back({100.0, 1.0, true});
    pts.push_back({200.0, 3.0, true});
    m.setPoints(pts);

    QCOMPARE(m.getCorrection(100.0), 1.0);
    QCOMPARE(m.getCorrection(200.0), 3.0);
    QCOMPARE(m.getCorrection(150.0), 2.0);
}

void TestCalibrationModel::getCorrection_twoSetPoints_clampsOutsideRange()
{
    CalibrationModel m;
    QVector<CalibrationPoint> pts;
    pts.push_back({100.0, 1.0, true});
    pts.push_back({200.0, 3.0, true});
    m.setPoints(pts);

    QCOMPARE(m.getCorrection(50.0),  1.0);
    QCOMPARE(m.getCorrection(250.0), 3.0);
}

void TestCalibrationModel::getCorrection_threeOrMoreSetPoints_passesThroughKnots()
{
    CalibrationModel m;
    QVector<CalibrationPoint> pts;
    pts.push_back({100.0, 1.0, true});
    pts.push_back({200.0, 2.0, true});
    pts.push_back({300.0, 5.0, true});
    pts.push_back({400.0, 4.0, true});
    m.setPoints(pts);

    // Cubic spline interpolation passes exactly through the knot values.
    QVERIFY(qAbs(m.getCorrection(100.0) - 1.0) < 1e-9);
    QVERIFY(qAbs(m.getCorrection(200.0) - 2.0) < 1e-9);
    QVERIFY(qAbs(m.getCorrection(300.0) - 5.0) < 1e-9);
    QVERIFY(qAbs(m.getCorrection(400.0) - 4.0) < 1e-9);

    // Mid-knot values must lie inside a reasonable envelope of the knots.
    const double mid = m.getCorrection(250.0);
    QVERIFY(mid > 1.0 && mid < 7.0);
}

// -----------------------------------------------------------------------------
//  Mode handling
// -----------------------------------------------------------------------------

void TestCalibrationModel::mode_defaultIsSimple()
{
    CalibrationModel m;
    QCOMPARE(m.mode(), CalibrationModel::Mode::Simple);
}

void TestCalibrationModel::modeAwareCorrection_disabledReturnsZero()
{
    CalibrationModel m;
    QVector<CalibrationPoint> pts{ {100.0, 5.0, true} };
    m.setPoints(pts);
    m.setMode(CalibrationModel::Mode::Disabled);
    QCOMPARE(m.getCorrection(100.0, 0.0), 0.0);
    QCOMPARE(m.getCorrection(100.0, -50.0), 0.0);
}

void TestCalibrationModel::modeAwareCorrection_simpleIgnoresMeasured()
{
    CalibrationModel m;
    QVector<CalibrationPoint> pts{
        {100.0, 1.0, true},
        {200.0, 2.0, true},
    };
    m.setPoints(pts);
    // Default mode is Simple; both measured values must give same answer
    // (linear interp on the freq axis only).
    const double a = m.getCorrection(150.0, -50.0);
    const double b = m.getCorrection(150.0, +5.0);
    QCOMPARE(a, b);
    QVERIFY(qAbs(a - 1.5) < 1e-9);
}

// -----------------------------------------------------------------------------
//  Advanced storage and bilinear interpolation
// -----------------------------------------------------------------------------

void TestCalibrationModel::advanced_powerAxisGen_clampsStepFloor()
{
    CalibrationModel m;
    // Asking for 1 dB step must be silently clamped to 2.5 dB (the floor).
    m.generateAdvancedPowerAxis(-30.0, -20.0, 1.0);
    QCOMPARE(m.advancedStepDb(), 2.5);
    const auto axis = m.advancedPowerAxis();
    QVERIFY(axis.size() >= 4);
    QCOMPARE(axis.first(), -30.0);
    QCOMPARE(axis.last(), -20.0);
}

void TestCalibrationModel::advanced_setCell_storesAndInterpolates()
{
    CalibrationModel m;
    m.setMode(CalibrationModel::Mode::Advanced);
    m.generateAdvancedPowerAxis(-30.0, -20.0, 2.5);
    m.addAdvancedFrequency(100.0);
    m.addAdvancedFrequency(200.0);
    // Fill every cell with a known value so interpolation is unambiguous.
    for (int r = 0; r < m.advancedRows().size(); ++r) {
        for (int p = 0; p < m.advancedPowerAxis().size(); ++p) {
            m.setAdvancedCell(r, p, 1.0);
        }
    }
    QCOMPARE(m.getCorrection(150.0, -25.0), 1.0);
}

void TestCalibrationModel::advanced_bilinearInterpolation_atCellExact()
{
    CalibrationModel m;
    m.setMode(CalibrationModel::Mode::Advanced);
    QVector<double> axis{-30.0, -27.5, -25.0};
    m.setAdvancedPowerAxis(axis);
    m.addAdvancedFrequency(100.0);
    m.addAdvancedFrequency(200.0);
    // freq 100, axis -30 = 3.0; freq 100, axis -25 = 4.0
    // freq 200, axis -30 = 5.0; freq 200, axis -25 = 8.0
    m.setAdvancedCell(0, 0, 3.0);  // freq=100, pwr=-30
    m.setAdvancedCell(0, 2, 4.0);  // freq=100, pwr=-25
    m.setAdvancedCell(1, 0, 5.0);  // freq=200, pwr=-30
    m.setAdvancedCell(1, 2, 8.0);  // freq=200, pwr=-25
    // Hit the exact corners:
    QCOMPARE(m.getCorrection(100.0, -30.0), 3.0);
    QCOMPARE(m.getCorrection(100.0, -25.0), 4.0);
    QCOMPARE(m.getCorrection(200.0, -30.0), 5.0);
    QCOMPARE(m.getCorrection(200.0, -25.0), 8.0);
}

void TestCalibrationModel::advanced_bilinearInterpolation_betweenCells()
{
    CalibrationModel m;
    m.setMode(CalibrationModel::Mode::Advanced);
    QVector<double> axis{-30.0, -25.0};
    m.setAdvancedPowerAxis(axis);
    m.addAdvancedFrequency(100.0);
    m.addAdvancedFrequency(200.0);
    m.setAdvancedCell(0, 0, 0.0);
    m.setAdvancedCell(0, 1, 2.0);
    m.setAdvancedCell(1, 0, 4.0);
    m.setAdvancedCell(1, 1, 8.0);
    // Centre point: bilinear average = 0+2+4+8 / 4 = 3.5
    const double v = m.getCorrection(150.0, -27.5);
    QVERIFY(qAbs(v - 3.5) < 1e-9);
}

void TestCalibrationModel::advanced_unfilledCell_fallsBackToNearestInRow()
{
    CalibrationModel m;
    m.setMode(CalibrationModel::Mode::Advanced);
    QVector<double> axis{-30.0, -27.5, -25.0};
    m.setAdvancedPowerAxis(axis);
    m.addAdvancedFrequency(100.0);
    // Only the middle cell is set; both edges fall back to it.
    m.setAdvancedCell(0, 1, 7.0);
    QCOMPARE(m.getCorrection(100.0, -30.0), 7.0);
    QCOMPARE(m.getCorrection(100.0, -27.5), 7.0);
    QCOMPARE(m.getCorrection(100.0, -25.0), 7.0);
}

void TestCalibrationModel::advanced_emptyTableReturnsZero()
{
    CalibrationModel m;
    m.setMode(CalibrationModel::Mode::Advanced);
    QCOMPARE(m.getCorrection(100.0, -25.0), 0.0);
}

// -----------------------------------------------------------------------------
//  JSON round-trip and legacy migration
// -----------------------------------------------------------------------------

void TestCalibrationModel::json_roundtripPreservesBothStores()
{
    CalibrationModel src;
    QVector<CalibrationPoint> pts{
        {100.0, 1.0, true}, {200.0, 2.0, true},
    };
    src.setPoints(pts);
    src.generateAdvancedPowerAxis(-30.0, -25.0, 2.5);
    src.addAdvancedFrequency(100.0);
    src.setAdvancedCell(0, 0, 9.0);
    src.setMode(CalibrationModel::Mode::Advanced);

    const QJsonObject root = src.saveToJson();
    CalibrationModel dst;
    QVERIFY(dst.loadFromJson(root));
    QCOMPARE(dst.mode(), CalibrationModel::Mode::Advanced);
    QCOMPARE(dst.getPoints().size(), 2);
    QCOMPARE(dst.advancedPowerAxis().size(), 3);
    QCOMPARE(dst.advancedRows().size(), 1);
    QVERIFY(dst.advancedRows().first().cells.first().isSet);
    QCOMPARE(dst.advancedRows().first().cells.first().correctionDb, 9.0);
}

void TestCalibrationModel::json_legacyProfileMigratesToSimple()
{
    // Old profile shape: top-level `name` + `points` array, no mode field.
    QJsonObject legacy;
    legacy["name"] = "old";
    QJsonArray arr;
    {
        QJsonObject p;
        p["frequencyMHz"] = 100.0;
        p["correctionDb"] = -2.0;
        p["isSet"]        = true;
        arr.append(p);
    }
    legacy["points"] = arr;

    CalibrationModel m;
    QVERIFY(m.loadFromJson(legacy));
    QCOMPARE(m.mode(), CalibrationModel::Mode::Simple);
    QCOMPARE(m.getPoints().size(), 1);
    QCOMPARE(m.getPoints().first().correctionDb, -2.0);
}

void TestCalibrationModel::modeSwitch_preservesBothStores()
{
    CalibrationModel m;
    // Populate both stores.
    QVector<CalibrationPoint> pts{ {100.0, 1.5, true} };
    m.setPoints(pts);
    m.generateAdvancedPowerAxis(-30.0, -25.0, 2.5);
    m.addAdvancedFrequency(100.0);
    m.setAdvancedCell(0, 0, 7.5);

    // Mode flips do not touch either store.
    m.setMode(CalibrationModel::Mode::Advanced);
    QCOMPARE(m.getPoints().size(), 1);
    QCOMPARE(m.getPoints().first().correctionDb, 1.5);
    QCOMPARE(m.advancedRows().first().cells.first().correctionDb, 7.5);

    m.setMode(CalibrationModel::Mode::Disabled);
    QCOMPARE(m.getPoints().first().correctionDb, 1.5);
    QCOMPARE(m.advancedRows().first().cells.first().correctionDb, 7.5);

    m.setMode(CalibrationModel::Mode::Simple);
    QCOMPARE(m.getPoints().first().correctionDb, 1.5);
    QCOMPARE(m.advancedRows().first().cells.first().correctionDb, 7.5);
}

QTEST_MAIN(TestCalibrationModel)
#include "test_calibrationmodel.moc"
