// Drive Rf8000Device::processData(QString) directly with canned strings.
// No serial port is opened. measurementReady is observed via QSignalSpy.
//
// The wire format the device understands is `$<dbm>dBm<vpp><unit>Vpp$`
// where <unit> is one character: 'm' keeps the value as-is, 'u' divides
// it by 1000.

#include <QTest>
#include <QSignalSpy>
#include "rf8000device.h"

namespace
{
PMDeviceProperties makeProps()
{
    PMDeviceProperties p;
    p.id = "rf8000";
    p.name = "RF8000";
    p.baudRate = 9600;
    return p;
}
}

class TestRf8000Parser : public QObject
{
    Q_OBJECT

private slots:
    void positiveReading_emitsMeasurement();
    void negativeReading_emitsMeasurement();
    void microVppUnit_dividesBy1000();
    void milliVppUnit_passesThrough();
    void multipleFramesInOnePacket_emitsOnePerFrame();
    void malformedFrame_emitsNoMeasurement();
};

void TestRf8000Parser::positiveReading_emitsMeasurement()
{
    Rf8000Device d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    d.processData(QStringLiteral("$+10.0dBm0.500mVpp$"));

    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toDouble(), 10.0);
    QCOMPARE(args.at(2).toDouble(), 0.5);
}

void TestRf8000Parser::negativeReading_emitsMeasurement()
{
    Rf8000Device d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    d.processData(QStringLiteral("$-30.5dBm0.020mVpp$"));

    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toDouble(), -30.5);
    QCOMPARE(args.at(2).toDouble(), 0.020);
}

void TestRf8000Parser::microVppUnit_dividesBy1000()
{
    Rf8000Device d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    d.processData(QStringLiteral("$-25.0dBm500uVpp$"));

    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toDouble(), -25.0);
    QCOMPARE(args.at(2).toDouble(), 0.5);
}

void TestRf8000Parser::milliVppUnit_passesThrough()
{
    Rf8000Device d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    d.processData(QStringLiteral("$-25.0dBm500mVpp$"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(2).toDouble(), 500.0);
}

void TestRf8000Parser::multipleFramesInOnePacket_emitsOnePerFrame()
{
    Rf8000Device d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    d.processData(QStringLiteral("$+05.0dBm0.100mVpp$$+06.0dBm0.110mVpp$"));

    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(0).at(1).toDouble(), 5.0);
    QCOMPARE(spy.at(1).at(1).toDouble(), 6.0);
}

void TestRf8000Parser::malformedFrame_emitsNoMeasurement()
{
    Rf8000Device d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy spyMeasurement(&d, &AbstractPMDevice::measurementReady);

    d.processData(QStringLiteral("noise without a dollar"));
    d.processData(QStringLiteral("$+0.5dBm"));     // truncated
    d.processData(QStringLiteral("0.1mVpp$"));     // missing prefix

    QCOMPARE(spyMeasurement.count(), 0);
}

QTEST_MAIN(TestRf8000Parser)
#include "test_rf8000_parser.moc"
