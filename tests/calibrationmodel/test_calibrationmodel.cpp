// Behaviour tests for CalibrationModel — focuses on the two non-Qt-model
// methods: generateFrequencies(start,end,step) and the correction lookup
// getCorrection(freqMHz), which has 0/1/2/>=3-point fallbacks around the
// cubic spline.

#include <QTest>
#include <QtMath>
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

QTEST_MAIN(TestCalibrationModel)
#include "test_calibrationmodel.moc"
