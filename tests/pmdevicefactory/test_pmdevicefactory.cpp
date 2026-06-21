// PMDeviceFactory tests for the RPM20-GS variant. Adds the
// concept_rf_rpm_binary entry to the dispatch table.

#include <QTest>
#include "pmdevicefactory.h"
#include "abstractpmdevice.h"
#include "rf8000device.h"
#include "rfpmv5device.h"
#include "rfpmv7device.h"
#include "conceptrfrpmdevice.h"

class TestPMDeviceFactory : public QObject
{
    Q_OBJECT

private slots:
    void propertiesForDevice_knownIds_match();
    void propertiesForDevice_unknownId_returnsDefault();

    void availableDevices_excludesDisabled();
    void availableDevices_sortedByMaxFreqDesc();
    void availableDevices_includesConceptRfRpm();

    void createDevice_dispatchesToConcreteType_data();
    void createDevice_dispatchesToConcreteType();
    void createDevice_unknownId_returnsNullptr();
};

void TestPMDeviceFactory::propertiesForDevice_knownIds_match()
{
    PMDeviceFactory f;

    QCOMPARE(f.propertiesForDevice("rf8000").id, QStringLiteral("rf8000"));
    QCOMPARE(f.propertiesForDevice("rf3000").id, QStringLiteral("rf3000"));
    QCOMPARE(f.propertiesForDevice("rf500").id,  QStringLiteral("rf500"));
    QCOMPARE(f.propertiesForDevice("rfpmv5").id, QStringLiteral("rfpmv5"));
    QCOMPARE(f.propertiesForDevice("rfpm_v7_10ghz").id, QStringLiteral("rfpm_v7_10ghz"));
    QCOMPARE(f.propertiesForDevice("concept_rf_rpm_binary").id,
             QStringLiteral("concept_rf_rpm_binary"));
}

void TestPMDeviceFactory::propertiesForDevice_unknownId_returnsDefault()
{
    PMDeviceFactory f;
    const PMDeviceProperties p = f.propertiesForDevice("does_not_exist");
    QVERIFY(p.id.isEmpty());
}

void TestPMDeviceFactory::availableDevices_excludesDisabled()
{
    PMDeviceFactory f;
    const auto list = f.availableDevices();
    for (const auto &p : list) {
        QVERIFY2(p.isEnabled, qPrintable(QStringLiteral("disabled device leaked into list: %1").arg(p.id)));
    }
    for (const auto &p : list) {
        QVERIFY(p.id != "rfpm_v7_10ghz");
    }
}

void TestPMDeviceFactory::availableDevices_sortedByMaxFreqDesc()
{
    PMDeviceFactory f;
    const auto list = f.availableDevices();
    QVERIFY(list.size() >= 2);
    for (int i = 1; i < list.size(); ++i) {
        QVERIFY2(list.at(i - 1).maxFreqHz >= list.at(i).maxFreqHz,
                 qPrintable(QStringLiteral("not sorted desc at %1: %2 < %3")
                            .arg(i).arg(list.at(i - 1).maxFreqHz).arg(list.at(i).maxFreqHz)));
    }
}

void TestPMDeviceFactory::availableDevices_includesConceptRfRpm()
{
    PMDeviceFactory f;
    const auto list = f.availableDevices();
    bool found = false;
    for (const auto &p : list) {
        if (p.id == "concept_rf_rpm_binary") { found = true; break; }
    }
    QVERIFY2(found, "concept_rf_rpm_binary missing from availableDevices()");
}

void TestPMDeviceFactory::createDevice_dispatchesToConcreteType_data()
{
    QTest::addColumn<QString>("deviceId");
    QTest::addColumn<QString>("expectedClassName");

    QTest::newRow("rf8000") << QStringLiteral("rf8000") << QStringLiteral("Rf8000Device");
    QTest::newRow("rf3000") << QStringLiteral("rf3000") << QStringLiteral("Rf8000Device");
    QTest::newRow("rf500")  << QStringLiteral("rf500")  << QStringLiteral("Rf8000Device");
    QTest::newRow("rfpmv5") << QStringLiteral("rfpmv5") << QStringLiteral("RfpmV5Device");
    QTest::newRow("rfpm_v7_10ghz") << QStringLiteral("rfpm_v7_10ghz") << QStringLiteral("RfpmV7Device");
    QTest::newRow("concept_rf_rpm_binary")
            << QStringLiteral("concept_rf_rpm_binary")
            << QStringLiteral("ConceptRfRpmDevice");
}

void TestPMDeviceFactory::createDevice_dispatchesToConcreteType()
{
    QFETCH(QString, deviceId);
    QFETCH(QString, expectedClassName);

    PMDeviceFactory f;
    AbstractPMDevice *d = f.createDevice(deviceId);
    QVERIFY(d != nullptr);
    QCOMPARE(QString(d->metaObject()->className()), expectedClassName);
    QCOMPARE(d->properties().id, deviceId);
    delete d;
}

void TestPMDeviceFactory::createDevice_unknownId_returnsNullptr()
{
    PMDeviceFactory f;
    QVERIFY(f.createDevice("does_not_exist") == nullptr);
}

QTEST_MAIN(TestPMDeviceFactory)
#include "test_pmdevicefactory.moc"
