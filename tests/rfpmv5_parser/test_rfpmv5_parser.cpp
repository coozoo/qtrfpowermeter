// Drive RfpmV5Device::processData(QString) directly with canned strings.
// No serial port is opened.
//
// The V5 protocol has two interleaved patterns in the incoming stream:
//   - `R<freq><sign><offset>A`     -- configuration readback, marks the
//                                     device as identified
//   - `<sign><DDD><DDDDD><unit>`   -- raw measurement samples; every 30th
//                                     match is accumulated and the
//                                     averaged dBm is flushed on the
//                                     sample timer tick
//
// onSampleTimerTimeout() is private — invoke it via QMetaObject so the
// tests do not need a real QTimer event.

#include <QTest>
#include <QSignalSpy>
#include "rfpmv5device.h"

namespace
{
PMDeviceProperties makeProps()
{
    PMDeviceProperties p;
    p.id = "rfpmv5";
    p.name = "RFPM V5";
    p.baudRate = 460800;
    return p;
}

void feedSampleFrames(RfpmV5Device &d, const QString &frame, int count)
{
    QString blob;
    for (int i = 0; i < count; ++i) blob += frame;
    d.processData(blob);
}
}

class TestRfpmV5Parser : public QObject
{
    Q_OBJECT

private slots:
    void configFrame_marksDeviceIdentified();
    void noMeasurement_belowSkipThreshold();
    void thirtiethSample_isAccumulated_andFlushedByTimer();
    void negativeReading_signApplied();
    void fragmentedDataAcrossCalls_eventuallyParses();
};

void TestRfpmV5Parser::configFrame_marksDeviceIdentified()
{
    RfpmV5Device d(makeProps());
    QSignalSpy logSpy(&d, &AbstractPMDevice::newLogMessage);

    d.processData(QStringLiteral("R1000+10.5A"));

    bool sawIdentified = false;
    for (const auto &args : logSpy) {
        if (args.at(0).toString().contains(QStringLiteral("identified successfully"))) {
            sawIdentified = true;
            break;
        }
    }
    QVERIFY(sawIdentified);
}

void TestRfpmV5Parser::noMeasurement_belowSkipThreshold()
{
    RfpmV5Device d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    // Feed 29 valid samples -- skip counter never reaches 30.
    feedSampleFrames(d, QStringLiteral("+05000500m"), 29);
    QMetaObject::invokeMethod(&d, "onSampleTimerTimeout", Qt::DirectConnection);

    QCOMPARE(spy.count(), 0);
}

void TestRfpmV5Parser::thirtiethSample_isAccumulated_andFlushedByTimer()
{
    RfpmV5Device d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    // 30 identical samples: "+050<DDDDD><unit>". dbm = "050" / 10 = 5.0
    feedSampleFrames(d, QStringLiteral("+05000500m"), 30);
    QMetaObject::invokeMethod(&d, "onSampleTimerTimeout", Qt::DirectConnection);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(1).toDouble(), 5.0);
}

void TestRfpmV5Parser::negativeReading_signApplied()
{
    RfpmV5Device d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    feedSampleFrames(d, QStringLiteral("-12500500m"), 30);
    QMetaObject::invokeMethod(&d, "onSampleTimerTimeout", Qt::DirectConnection);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(1).toDouble(), -12.5);
}

void TestRfpmV5Parser::fragmentedDataAcrossCalls_eventuallyParses()
{
    RfpmV5Device d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy spy(&d, &AbstractPMDevice::measurementReady);

    // Send the first sample split into two chunks, then 29 more whole
    // samples; that totals 30 and crosses the skip threshold once.
    d.processData(QStringLiteral("+0500"));
    d.processData(QStringLiteral("0500m"));
    feedSampleFrames(d, QStringLiteral("+05000500m"), 29);
    QMetaObject::invokeMethod(&d, "onSampleTimerTimeout", Qt::DirectConnection);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(1).toDouble(), 5.0);
}

QTEST_MAIN(TestRfpmV5Parser)
#include "test_rfpmv5_parser.moc"
