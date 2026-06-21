// Sanity tests on every PMDeviceProperties entry registered in the
// RPM20-GS variant of PMDeviceFactory.

#include <QTest>
#include "pmdevicefactory.h"

class TestPropertiesValidation : public QObject
{
    Q_OBJECT

private slots:
    void everyKnownIdHasNonEmptyName();
    void freqRangeIsAscending();
    void powerRangeIsAscending();
    void baudRateIsPositive();
    void internalAttenuatorRangeIsConsistent();

private:
    static QStringList knownIds()
    {
        return {
            QStringLiteral("rf500"),
            QStringLiteral("rf3000"),
            QStringLiteral("rf8000"),
            QStringLiteral("rfpmv5"),
            QStringLiteral("rfpm_v7_10ghz"),
            QStringLiteral("concept_rf_rpm_binary"),
        };
    }
};

void TestPropertiesValidation::everyKnownIdHasNonEmptyName()
{
    PMDeviceFactory f;
    for (const QString &id : knownIds()) {
        const PMDeviceProperties p = f.propertiesForDevice(id);
        QVERIFY2(!p.name.isEmpty(), qPrintable(QStringLiteral("empty name for %1").arg(id)));
    }
}

void TestPropertiesValidation::freqRangeIsAscending()
{
    PMDeviceFactory f;
    for (const QString &id : knownIds()) {
        const PMDeviceProperties p = f.propertiesForDevice(id);
        QVERIFY2(p.minFreqHz < p.maxFreqHz,
                 qPrintable(QStringLiteral("%1: minFreq %2 >= maxFreq %3")
                            .arg(id).arg(p.minFreqHz).arg(p.maxFreqHz)));
    }
}

void TestPropertiesValidation::powerRangeIsAscending()
{
    PMDeviceFactory f;
    for (const QString &id : knownIds()) {
        const PMDeviceProperties p = f.propertiesForDevice(id);
        QVERIFY2(p.minPowerDbm < p.maxPowerDbm,
                 qPrintable(QStringLiteral("%1: minPower %2 >= maxPower %3")
                            .arg(id).arg(p.minPowerDbm).arg(p.maxPowerDbm)));
    }
}

void TestPropertiesValidation::baudRateIsPositive()
{
    PMDeviceFactory f;
    for (const QString &id : knownIds()) {
        const PMDeviceProperties p = f.propertiesForDevice(id);
        QVERIFY2(p.baudRate > 0,
                 qPrintable(QStringLiteral("%1: invalid baudRate %2").arg(id).arg(p.baudRate)));
    }
}

void TestPropertiesValidation::internalAttenuatorRangeIsConsistent()
{
    PMDeviceFactory f;
    for (const QString &id : knownIds()) {
        const PMDeviceProperties p = f.propertiesForDevice(id);
        if (!p.hasInternalAttenuator) continue;
        QVERIFY2(p.internalAttMinDb <= p.internalAttMaxDb,
                 qPrintable(QStringLiteral("%1: internal att min %2 > max %3")
                            .arg(id).arg(p.internalAttMinDb).arg(p.internalAttMaxDb)));
        QVERIFY2(p.internalAttStepDb > 0.0,
                 qPrintable(QStringLiteral("%1: internal att step %2 not positive")
                            .arg(id).arg(p.internalAttStepDb)));
    }
}

QTEST_MAIN(TestPropertiesValidation)
#include "test_properties_validation.moc"
