// Pure-logic tests for AttDevice's serial-line parser. Inject lines via
// the SerialPortInterface::serialPortNewData signal (which the device's
// own ctor connects to its onSerialPortNewData slot) and watch outbound
// signals through QSignalSpy.
//
// Primary purpose: pin the per-instance m_buffer contract so that the
// function-static buffer regression cannot reappear (the same defect
// CLAUDE.md documents for RfpmV7Device).

#include <QTest>
#include <QSignalSpy>
#include "attdevice.h"

class TestAttDeviceParser : public QObject
{
    Q_OBJECT

private slots:
    void bufferIsPerInstance_noCrossInstanceLeak();
    void bufferClearedOnPortClose();
};

void TestAttDeviceParser::bufferIsPerInstance_noCrossInstanceLeak()
{
    // Instance A receives an "ATT = -" fragment with no trailing newline
    // or digits. A function-static buffer would leave "ATT = -" sitting in
    // shared state. Instance B then receives just "10.50" -- on its own
    // clean buffer that does not match the parser's regex, so no
    // currentValueChanged should fire. If we instead see B emit the joined
    // "-10.50", A's tail leaked into B.
    AttDevice a;
    emit a.serialPortNewData(QStringLiteral("ATT = -"));

    AttDevice b;
    QSignalSpy spy(&b, &AttDevice::currentValueChanged);
    emit b.serialPortNewData(QStringLiteral("10.50"));

    QVERIFY2(spy.isEmpty(),
             "instance B emitted a value -- instance A's parser tail leaked across instances");
}

void TestAttDeviceParser::bufferClearedOnPortClose()
{
    // Feed a partial frame, simulate a port close, then feed a new fragment.
    // The half-frame from before the close must NOT fuse with the new data.
    AttDevice a;
    emit a.serialPortNewData(QStringLiteral("ATT = -"));

    // Port close handler is private; trigger it via the same signal AttDevice
    // listens to. SerialPortInterface emits portClosed when the port stops.
    emit a.portClosed();

    QSignalSpy spy(&a, &AttDevice::currentValueChanged);
    emit a.serialPortNewData(QStringLiteral("10.50"));

    QVERIFY2(spy.isEmpty(),
             "tail from before portClosed fused with the next session");
}

QTEST_MAIN(TestAttDeviceParser)
#include "test_attdevice_parser.moc"
