// Drive RfpmV7Device::processData(QString) with canned strings.
//
// The V7 protocol is line-based:
//     Level: -10.5dBm     <CR><LF>
//     Power: 100.0uW       <CR><LF>
// The dBm goes out unchanged on the next valid Power line; the unit on
// Power (uW/nW/pW/mW/W) is appended with no separator (matching the
// regex `Power:\s*([0-9.]+)(\wW)` in rfpmv7device.cpp) and determines how
// the value is converted to milliwatts and then to Vpp via UnitConverter.
//
// Per-instance buffer (m_buffer) holds the trailing partial line between
// processData calls. The dedicated test below pins that state does NOT
// leak across instances (it used to: the buffer was function-static).

#include <QTest>
#include <QSignalSpy>
#include "rfpmv7device.h"
#include "unitconverter.h"

namespace
{
PMDeviceProperties makeProps()
{
    PMDeviceProperties p;
    p.id = "rfpm_v7_10ghz";
    p.name = "RF-PM V7";
    p.baudRate = 115200;
    p.hasInternalAttenuator = true;
    return p;
}
}

class TestRfpmV7Parser : public QObject
{
    Q_OBJECT

private slots:
    void levelPlusPower_emitsMeasurement_data();
    void levelPlusPower_emitsMeasurement();
    void powerWithoutLevel_doesNotEmit();
    void perInstanceBuffer_noCrossInstanceLeak();
};

void TestRfpmV7Parser::levelPlusPower_emitsMeasurement_data()
{
    QTest::addColumn<QString>("powerLine");
    QTest::addColumn<double>("expectedDbm");
    QTest::addColumn<double>("expectedMilliwatts");

    QTest::newRow("mW")
            << QStringLiteral("Power: 1.0mW\r\n")
            << -10.5 << 1.0;
    QTest::newRow("uW")
            << QStringLiteral("Power: 100.0uW\r\n")
            << -10.5 << 0.1;
    QTest::newRow("nW")
            << QStringLiteral("Power: 500.0nW\r\n")
            << -10.5 << 0.0005;
    // Note: the regex `\wW` requires a word char immediately before W,
    // so a bare "W" unit cannot match without a leading letter. Test only
    // the prefixed units.
}

void TestRfpmV7Parser::levelPlusPower_emitsMeasurement()
{
    QFETCH(QString, powerLine);
    QFETCH(double, expectedDbm);
    QFETCH(double, expectedMilliwatts);

    RfpmV7Device d(makeProps());
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    d.processData(QStringLiteral("Level: -10.5dBm\r\n"));
    d.processData(powerLine);

    QCOMPARE(spy.count(), 1);
    const auto args = spy.takeFirst();
    QCOMPARE(args.at(1).toDouble(), expectedDbm);
    QVERIFY(qAbs(args.at(2).toDouble() - UnitConverter::milliwattsToVpp(expectedMilliwatts)) < 1e-6);
}

void TestRfpmV7Parser::powerWithoutLevel_doesNotEmit()
{
    RfpmV7Device d(makeProps());
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    // Flush the static buffer with a trailing newline, then send a Power
    // line on its own. With no preceding Level, m_waitingForPower is false
    // and nothing is emitted.
    d.processData(QStringLiteral("ignored line\r\n"));
    d.processData(QStringLiteral("Power: 1.0mW\r\n"));

    QCOMPARE(spy.count(), 0);
}

void TestRfpmV7Parser::perInstanceBuffer_noCrossInstanceLeak()
{
    // Instance A receives a Level line plus a *truncated* Power line.
    // Its trailing "Power: 1.0m" lives in A's per-instance m_buffer.
    // Instance B, asked to parse "W\r\n" on its own clean buffer, must
    // see only "W" (logged as non-measurement) and NOT "Power: 1.0mW"
    // (which would mean A's tail leaked into B). Regression guard for
    // the original function-static buffer.
    RfpmV7Device a(makeProps());
    a.processData(QStringLiteral("Level: -10.5dBm\r\nPower: 1.0m"));

    RfpmV7Device b(makeProps());
    QSignalSpy logSpy(&b, &AbstractPMDevice::newLogMessage);
    b.processData(QStringLiteral("W\r\n"));

    bool sawLeakedPower = false;
    for (const auto &args : logSpy) {
        if (args.at(0).toString().contains(QStringLiteral("Power: 1.0mW"))) {
            sawLeakedPower = true;
            break;
        }
    }
    QVERIFY2(!sawLeakedPower,
             "instance B saw data left behind by instance A -- m_buffer is leaking");
}

QTEST_MAIN(TestRfpmV7Parser)
#include "test_rfpmv7_parser.moc"
