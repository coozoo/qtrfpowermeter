// CableModel parses a JSON description (manufacturer, name, attenuation
// table) and exposes getAttenuationPer100m(freqMHz) backed by a cubic
// spline that the constructor builds. These tests build canned JSON
// fixtures and verify the model's metadata, spline accuracy at the knots,
// and the degenerate-input fallbacks.

#include <QTest>
#include <QJsonObject>
#include <QJsonValue>
#include <QtMath>
#include "cablemodel.h"

class TestCableModel : public QObject
{
    Q_OBJECT

private slots:
    void parsesMetadata();
    void splineAtKnots();
    void splineMonotonicBetweenKnots();
    void emptyAttenuation_returnsZero();
    void missingAttenuationKey_returnsZero();
};

namespace
{
QJsonObject makeCable(const QJsonObject &attenuation)
{
    QJsonObject o;
    o.insert("name", "TestCable");
    o.insert("manufacturer", "Acme");
    o.insert("type", "Coax");
    o.insert("datasheet", "https://example.invalid/datasheet.pdf");
    o.insert("extrainfo", QJsonObject{ {"impedance", 50.0} });
    o.insert("attenuation", attenuation);
    return o;
}
}

void TestCableModel::parsesMetadata()
{
    QJsonObject attenuation;
    attenuation.insert("100",  QJsonValue(1.0));
    attenuation.insert("1000", QJsonValue(3.0));
    attenuation.insert("3000", QJsonValue(6.0));

    CableModel m(makeCable(attenuation));

    QCOMPARE(m.getName(), QStringLiteral("TestCable"));
    QCOMPARE(m.getManufacturer(), QStringLiteral("Acme"));
    QCOMPARE(m.getType(), QStringLiteral("Coax"));
    QCOMPARE(m.getDataSource(), QStringLiteral("https://example.invalid/datasheet.pdf"));
    QCOMPARE(m.getAdditionalInfo().value("impedance").toDouble(), 50.0);

    // setupSpline prepends (0,0) then reports m_minFrequency as freqs[1] -> 100,
    // and the largest freq as m_maxFrequency -> 3000.
    QCOMPARE(m.getMinFrequency(), 100.0);
    QCOMPARE(m.getMaxFrequency(), 3000.0);
}

void TestCableModel::splineAtKnots()
{
    QJsonObject attenuation;
    attenuation.insert("100",  QJsonValue(1.0));
    attenuation.insert("1000", QJsonValue(3.0));
    attenuation.insert("3000", QJsonValue(6.0));
    attenuation.insert("6000", QJsonValue(9.0));

    CableModel m(makeCable(attenuation));

    // Cubic spline must pass exactly through each knot value (and the (0,0)
    // anchor that setupSpline injects).
    QVERIFY(qAbs(m.getAttenuationPer100m(0.0)    - 0.0) < 1e-9);
    QVERIFY(qAbs(m.getAttenuationPer100m(100.0)  - 1.0) < 1e-9);
    QVERIFY(qAbs(m.getAttenuationPer100m(1000.0) - 3.0) < 1e-9);
    QVERIFY(qAbs(m.getAttenuationPer100m(3000.0) - 6.0) < 1e-9);
    QVERIFY(qAbs(m.getAttenuationPer100m(6000.0) - 9.0) < 1e-9);
}

void TestCableModel::splineMonotonicBetweenKnots()
{
    QJsonObject attenuation;
    attenuation.insert("100",  QJsonValue(1.0));
    attenuation.insert("1000", QJsonValue(3.0));
    attenuation.insert("3000", QJsonValue(6.0));
    attenuation.insert("6000", QJsonValue(9.0));

    CableModel m(makeCable(attenuation));

    // Between 1000 and 3000 the attenuation rises 3 -> 6. The spline should
    // produce intermediate values inside a tolerant envelope and stay above
    // the left knot value.
    const double mid = m.getAttenuationPer100m(2000.0);
    QVERIFY(mid > 3.0 && mid < 7.0);
}

void TestCableModel::emptyAttenuation_returnsZero()
{
    QJsonObject empty;
    CableModel m(makeCable(empty));

    // No knots: spline is invalid -> returns 0.0 regardless of frequency.
    QCOMPARE(m.getAttenuationPer100m(100.0), 0.0);
    QCOMPARE(m.getAttenuationPer100m(1000.0), 0.0);
}

void TestCableModel::missingAttenuationKey_returnsZero()
{
    // Cable described with no attenuation table at all.
    QJsonObject o;
    o.insert("name", "Bare");
    CableModel m(o);

    QCOMPARE(m.getAttenuationPer100m(500.0), 0.0);
    QCOMPARE(m.getName(), QStringLiteral("Bare"));
}

QTEST_APPLESS_MAIN(TestCableModel)
#include "test_cablemodel.moc"
