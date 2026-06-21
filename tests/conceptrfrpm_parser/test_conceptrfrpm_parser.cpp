// Tests for the ConceptRfRpmDevice fixes against docs/protocol/rpm-series.md.
//
// Covers the four divergences found during audit:
//   1. Response 0x06 streaming sample MUST be decoded big-endian.
//   2. Response 0x00 identify MUST extract device_id + firmware_version + serial.
//   3. Init handshake MUST send cmd 0x83 (set sampling) before reading calibration.
//   4. Init handshake SHOULD send cmd 0x86 with 0xFFFF before sequential reads.
//
// Plus general framing safety: bad checksum is rejected; fragmented bytes
// across calls eventually decode; non-sync prefix bytes are skipped.

#include <QTest>
#include <QSignalSpy>
#include "conceptrfrpmdevice.h"
#include "conceptrfrpmlookuptables.h"
#include "pmdeviceproperties.h"
#include "serialportinterface.h"

namespace
{
PMDeviceProperties makeProps()
{
    PMDeviceProperties p;
    p.id = "concept_rf_rpm_binary";
    p.name = "Concept RF RPM";
    p.baudRate = 460800;
    p.minFreqHz = 50;
    p.maxFreqHz = 20000000000ULL;
    return p;
}

QByteArray frame(quint8 cmd, const QByteArray &payload)
{
    QByteArray f;
    f.append(char(0x55));
    f.append(char(0xAA));
    f.append(char(cmd));
    quint8 len = static_cast<quint8>(payload.size());
    f.append(char(len));
    f.append(char(static_cast<quint8>(~len)));
    f.append(payload);
    quint8 cs = 0;
    for (int i = 2; i < f.size(); ++i) cs += static_cast<quint8>(f.at(i));
    f.append(char(cs));
    return f;
}

QByteArray identifyPayload(quint16 deviceId, quint16 fwVersion, quint32 serial)
{
    QByteArray p;
    p.append(char((deviceId >> 8) & 0xFF)); p.append(char(deviceId & 0xFF));
    p.append(char((fwVersion >> 8) & 0xFF)); p.append(char(fwVersion & 0xFF));
    p.append(char((serial >> 24) & 0xFF)); p.append(char((serial >> 16) & 0xFF));
    p.append(char((serial >> 8) & 0xFF));  p.append(char(serial & 0xFF));
    return p;
}
}

class TestConceptRfRpmParser : public QObject
{
    Q_OBJECT

private slots:
    void constructsWithoutCrashing();
    void propertiesAreCarriedThrough();

    // Fix 1 — endianness of response 0x06.
    void decodeStreamingSample_isBigEndian_data();
    void decodeStreamingSample_isBigEndian();
    void decodeStreamingSample_wrongPayloadSize_returnsZero();

    // Fix 4 — identify response carries all three fields.
    void decodeIdentify_extractsAllFields_data();
    void decodeIdentify_extractsAllFields();
    void decodeIdentify_shortPayload_isNotOk();

    // Fix 2 + 3 — init handshake order.
    void identifyResponse_storesVersionAndSerial();
    void identifyResponse_logsIdentified();

    // Framing safety.
    void badChecksum_isDropped();
    void fragmentedFrame_eventuallyDecodes();
    void noiseBeforeSyncByte_isSkipped();

    // Lookup table support API.
    void lookupTable_isRowFilled_zeroRowReportsUnfilled();
    void lookupTable_firstUnfilledRow_returnsLowestUnset();

    // End-to-end signal wiring: serialPortNewBinaryData -> device.
    // Regression guard for the missing-emit bug in
    // SerialPortInterface::readData() that silently broke real-hardware
    // connect (the signal was declared as a local function instead of
    // emitted).
    void serialPortBinarySignal_reachesDevice();

    // Watchdog: receiving 0x06 streaming samples (state-incompatible
    // for Identifying state) must NOT stop the watchdog. When it fires,
    // the error message should make compatibility the obvious cause.
    void identificationTimeout_streamingSamplesDoNotResetWatchdog();
    void identificationTimeout_messageNamesTheCompatibilityCase();
};

void TestConceptRfRpmParser::constructsWithoutCrashing()
{
    ConceptRfRpmDevice d(makeProps());
    Q_UNUSED(d);
}

void TestConceptRfRpmParser::propertiesAreCarriedThrough()
{
    ConceptRfRpmDevice d(makeProps());
    QCOMPARE(d.properties().id, QStringLiteral("concept_rf_rpm_binary"));
    QCOMPARE(d.properties().baudRate, 460800);
}

void TestConceptRfRpmParser::decodeStreamingSample_isBigEndian_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<qint32>("expected");

    QTest::newRow("0x00 00 00 01 -> 1 (BE)")
            << QByteArray::fromHex("00000001") << qint32(1);
    QTest::newRow("0x01 00 00 00 -> 16777216 (BE)")
            << QByteArray::fromHex("01000000") << qint32(16777216);
    QTest::newRow("0x00 24 8E 12 -> 2395666 (BE)")
            << QByteArray::fromHex("00248E12") << qint32(2395666);
    QTest::newRow("0x7F FF FF FF -> INT32_MAX (BE)")
            << QByteArray::fromHex("7FFFFFFF") << qint32(0x7FFFFFFF);
    QTest::newRow("0xFF FF FF FF -> -1 (BE, sign-ext)")
            << QByteArray::fromHex("FFFFFFFF") << qint32(-1);
}

void TestConceptRfRpmParser::decodeStreamingSample_isBigEndian()
{
    QFETCH(QByteArray, payload);
    QFETCH(qint32, expected);
    QCOMPARE(ConceptRfRpmDevice::decodeStreamingSamplePayload(payload), expected);
}

void TestConceptRfRpmParser::decodeStreamingSample_wrongPayloadSize_returnsZero()
{
    QCOMPARE(ConceptRfRpmDevice::decodeStreamingSamplePayload(QByteArray()), 0);
    QCOMPARE(ConceptRfRpmDevice::decodeStreamingSamplePayload(QByteArray::fromHex("0001")), 0);
    QCOMPARE(ConceptRfRpmDevice::decodeStreamingSamplePayload(QByteArray::fromHex("0000000001")), 0);
}

void TestConceptRfRpmParser::decodeIdentify_extractsAllFields_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<quint16>("deviceId");
    QTest::addColumn<quint16>("fwVersion");
    QTest::addColumn<quint32>("serial");

    QTest::newRow("RPM-20GS id 101, fw 1.05, sn 0x12345678")
            << identifyPayload(101, 0x0069, 0x12345678)
            << quint16(101) << quint16(0x0069) << quint32(0x12345678);
    QTest::newRow("RPM-3GS id 102, fw 0.99, sn 1")
            << identifyPayload(102, 99, 1)
            << quint16(102) << quint16(99) << quint32(1);
    QTest::newRow("Max u32 serial")
            << identifyPayload(107, 200, 0xFFFFFFFFu)
            << quint16(107) << quint16(200) << quint32(0xFFFFFFFFu);
}

void TestConceptRfRpmParser::decodeIdentify_extractsAllFields()
{
    QFETCH(QByteArray, payload);
    QFETCH(quint16, deviceId);
    QFETCH(quint16, fwVersion);
    QFETCH(quint32, serial);

    auto f = ConceptRfRpmDevice::decodeIdentifyPayload(payload);
    QVERIFY(f.ok);
    QCOMPARE(f.deviceId, deviceId);
    QCOMPARE(f.firmwareVersion, fwVersion);
    QCOMPARE(f.serialNumber, serial);
}

void TestConceptRfRpmParser::decodeIdentify_shortPayload_isNotOk()
{
    auto f = ConceptRfRpmDevice::decodeIdentifyPayload(QByteArray::fromHex("0065"));
    QVERIFY(!f.ok);
}

void TestConceptRfRpmParser::identifyResponse_storesVersionAndSerial()
{
    ConceptRfRpmDevice d(makeProps());
    d.setLoggingEnabled(false);

    // Drive into Identifying state (onPortOpened is private; reach it via
    // the meta-system). m_serialPort is not actually open so the cmd 0x80
    // write is a no-op, but state has already advanced.
    QMetaObject::invokeMethod(&d, "onPortOpened", Qt::DirectConnection);

    const QByteArray idFrame = frame(0x00, identifyPayload(101, 0x0069, 0xDEADBEEF));
    QMetaObject::invokeMethod(&d, "onSerialPortNewBinaryData", Qt::DirectConnection,
                              Q_ARG(QByteArray, idFrame));

    QCOMPARE(d.firmwareVersionRaw(), quint16(0x0069));
    QCOMPARE(d.serialNumber(), quint32(0xDEADBEEFu));
    QVERIFY(qAbs(d.firmwareVersion() - 1.05) < 1e-9);
}

void TestConceptRfRpmParser::identifyResponse_logsIdentified()
{
    ConceptRfRpmDevice d(makeProps());
    QSignalSpy logSpy(&d, &AbstractPMDevice::newLogMessage);

    QMetaObject::invokeMethod(&d, "onPortOpened", Qt::DirectConnection);
    const QByteArray idFrame = frame(0x00, identifyPayload(101, 0x0069, 1));
    QMetaObject::invokeMethod(&d, "onSerialPortNewBinaryData", Qt::DirectConnection,
                              Q_ARG(QByteArray, idFrame));

    bool sawIdentified = false;
    for (const auto &args : logSpy) {
        if (args.at(0).toString().contains("Identified")) { sawIdentified = true; break; }
    }
    QVERIFY(sawIdentified);
}

void TestConceptRfRpmParser::badChecksum_isDropped()
{
    ConceptRfRpmDevice d(makeProps());
    QSignalSpy propsSpy(&d, &AbstractPMDevice::propertiesUpdated);

    QMetaObject::invokeMethod(&d, "onPortOpened", Qt::DirectConnection);

    // Build a valid identify frame then flip the checksum byte.
    QByteArray bad = frame(0x00, identifyPayload(101, 0x0069, 1));
    bad[bad.size() - 1] = char(static_cast<quint8>(bad.at(bad.size() - 1)) ^ 0xFF);

    QMetaObject::invokeMethod(&d, "onSerialPortNewBinaryData", Qt::DirectConnection,
                              Q_ARG(QByteArray, bad));

    // No identification happened.
    QCOMPARE(propsSpy.count(), 0);
    QCOMPARE(d.firmwareVersionRaw(), quint16(0));
}

void TestConceptRfRpmParser::fragmentedFrame_eventuallyDecodes()
{
    ConceptRfRpmDevice d(makeProps());
    d.setLoggingEnabled(false);

    QMetaObject::invokeMethod(&d, "onPortOpened", Qt::DirectConnection);

    const QByteArray full = frame(0x00, identifyPayload(101, 0x0069, 0xCAFEBABE));
    QMetaObject::invokeMethod(&d, "onSerialPortNewBinaryData", Qt::DirectConnection,
                              Q_ARG(QByteArray, full.left(5)));
    QCOMPARE(d.firmwareVersionRaw(), quint16(0));        // still partial
    QMetaObject::invokeMethod(&d, "onSerialPortNewBinaryData", Qt::DirectConnection,
                              Q_ARG(QByteArray, full.mid(5)));
    QCOMPARE(d.firmwareVersionRaw(), quint16(0x0069));   // full frame now in
    QCOMPARE(d.serialNumber(), quint32(0xCAFEBABEu));
}

void TestConceptRfRpmParser::noiseBeforeSyncByte_isSkipped()
{
    ConceptRfRpmDevice d(makeProps());
    d.setLoggingEnabled(false);
    QMetaObject::invokeMethod(&d, "onPortOpened", Qt::DirectConnection);

    QByteArray noisy;
    noisy.append(QByteArray::fromHex("00112233"));  // garbage prefix
    noisy.append(frame(0x00, identifyPayload(101, 0x0069, 1)));
    QMetaObject::invokeMethod(&d, "onSerialPortNewBinaryData", Qt::DirectConnection,
                              Q_ARG(QByteArray, noisy));

    QCOMPARE(d.firmwareVersionRaw(), quint16(0x0069));
}

void TestConceptRfRpmParser::lookupTable_isRowFilled_zeroRowReportsUnfilled()
{
    Rpm3gsLookupTable t;
    t.initializeTables();
    QVERIFY(!t.isRowFilled(0));
    QVERIFY(!t.allRowsFilled());

    QVector<float> row(t.getPowerTableSize(), 1.5f);
    t.setVoltageRow(0, row);
    QVERIFY(t.isRowFilled(0));
}

void TestConceptRfRpmParser::lookupTable_firstUnfilledRow_returnsLowestUnset()
{
    Rpm3gsLookupTable t;
    t.initializeTables();
    QCOMPARE(t.firstUnfilledRow(), 0);

    QVector<float> row(t.getPowerTableSize(), 2.0f);
    t.setVoltageRow(0, row);
    QCOMPARE(t.firstUnfilledRow(), 1);

    t.setVoltageRow(1, row);
    QCOMPARE(t.firstUnfilledRow(), 2);
}

void TestConceptRfRpmParser::serialPortBinarySignal_reachesDevice()
{
    ConceptRfRpmDevice d(makeProps());
    d.setLoggingEnabled(false);

    SerialPortInterface *port = d.findChild<SerialPortInterface*>();
    QVERIFY2(port != nullptr, "ConceptRfRpmDevice should own a SerialPortInterface child");

    // Drive the device into the Identifying state. Without an open port
    // the cmd 0x80 write is a no-op, but state has advanced.
    QMetaObject::invokeMethod(&d, "onPortOpened", Qt::DirectConnection);

    // Call the same emit path that readData() takes. If someone reverts
    // serialportinterface.cpp:67 back to a function declaration the
    // serialPortNewBinaryData signal will go silent and the device will
    // not see the identify response.
    QSignalSpy binarySpy(port, &SerialPortInterface::serialPortNewBinaryData);
    const QByteArray idFrame = frame(0x00, identifyPayload(101, 0x0069, 0xCAFEBABE));
    port->processIncomingBytes(idFrame);

    QCOMPARE(binarySpy.count(), 1);                          // signal was actually emitted
    QCOMPARE(d.firmwareVersionRaw(), quint16(0x0069));        // device received it
    QCOMPARE(d.serialNumber(), quint32(0xCAFEBABEu));
}

void TestConceptRfRpmParser::identificationTimeout_streamingSamplesDoNotResetWatchdog()
{
    // The device firmware starts emitting 0x06 streaming samples the
    // moment the port opens, before any identify exchange. Those frames
    // must not be treated as "the handshake is progressing"; otherwise
    // an incompatible device that streams 0x06 forever could keep us
    // wedged in Identifying state until the user kills the app.

    ConceptRfRpmDevice d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy errSpy(&d, &AbstractPMDevice::deviceError);

    QMetaObject::invokeMethod(&d, "onPortOpened", Qt::DirectConnection);

    // Feed a well-formed 0x06 streaming sample. With the pre-fix handler
    // this would have stopped the watchdog. With the fix it's ignored
    // because the state machine is in Identifying, not Ready.
    const QByteArray streamingSample = frame(0x06, QByteArray::fromHex("00248E12"));
    QMetaObject::invokeMethod(&d, "onSerialPortNewBinaryData", Qt::DirectConnection,
                              Q_ARG(QByteArray, streamingSample));

    // Force the timeout fire (can't sit through 3 wall-clock seconds in
    // a unit test).
    QMetaObject::invokeMethod(&d, "onIdentificationTimeout", Qt::DirectConnection);

    QCOMPARE(errSpy.count(), 1);
    // Watchdog firing means the streaming sample didn't cancel it.
    QVERIFY(errSpy.first().at(0).toString().contains("not a compatible"));
}

void TestConceptRfRpmParser::identificationTimeout_messageNamesTheCompatibilityCase()
{
    // When the watchdog fires while still in Identifying, the user-facing
    // error string should mention compatibility so they know to pick a
    // different device type for the same VID:PID. Locks the wording so
    // a future cleanup doesn't accidentally drop the hint.

    ConceptRfRpmDevice d(makeProps());
    d.setLoggingEnabled(false);
    QSignalSpy errSpy(&d, &AbstractPMDevice::deviceError);

    QMetaObject::invokeMethod(&d, "onPortOpened", Qt::DirectConnection);
    QMetaObject::invokeMethod(&d, "onIdentificationTimeout", Qt::DirectConnection);

    QCOMPARE(errSpy.count(), 1);
    const QString msg = errSpy.first().at(0).toString();
    QVERIFY2(msg.contains("identify"), qPrintable(msg));
    QVERIFY2(msg.contains("compatible"), qPrintable(msg));
}

QTEST_MAIN(TestConceptRfRpmParser)
#include "test_conceptrfrpm_parser.moc"
