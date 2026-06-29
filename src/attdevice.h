/*
 * model: DC-6GHZ-30DBpp
 * baudrate: 9600(115200 usb no need to be set)
 * step: 0.25
 * max: 31.75
 * -set: "att-00.00\r\n"
 * -response: "attOK"
 * -read data: "READ\r\n"
 * -response: "ATT = -31.75"

 * model: DC-6GHZ-90DB-V3
 * baudrate: 9600(115200 usb no need to be set)
 * step: 0.25
 * max: 95.25
 * -set: "att-000.00\r\n"
 * -response: "attOK"
 * -read data: "READ\r\n"
 * -response: "ATT = -31.75"

 * model: DC-3G-90DB-V2
 * baudrate: 9600(115200 usb no need to be set)
 * step: 0.5
 * max: 94.5
 * -set: "att-000.00\r\n"
 * -response: "attOK"
 * -read data: "READ\r\n"
 * -response: "ATT = -31.75"

 * model: DC-8GHZ-30DB-0.1DB
 * baudrate: 9600(115200 usb no need to be set)
 * step: 0.1
 * max: 30
 * -set: "att-00.00\r\n"
 * -response: "attOK"
 * -read data: "READ\r\n"
 * -response: "ATT = -30.00"

 * model: DC-6GHZ-120DB
 * baudrate: 9600(115200 usb no need to be set)
 * step: 0.25
 * max: 124.75
 * -set: "att-000.00\r\n"
 * -response: "attOK"
 * -read data: "READ\r\n"
 * -response: "ATT = -31.75"

 * there is such similar devices I'm not sure how to identify them
 * I found some descriptions of their comunication
 * but in fact I'm not sure about that
 * it's possible to implement them only when own such device
 * ??????
 * ATT-6000
 * "wv03125\r\n"
 */

#ifndef ATTDEVICE_H
#define ATTDEVICE_H

#include "serialportinterface.h"
#include <QString>
#include <QTimer>
#include <QRegularExpression>
#include <QDebug>
#include <QList>
#include <limits>

constexpr int hardwareReadIntervalMs = 5000;

enum class AttFormat
{
    Format000_00, // "att-%06.2f\r\n"
    Format00_00,  // "att-%05.2f\r\n"
    Unknown
};
inline QString formatToString(AttFormat fmt)
{
    switch (fmt)
        {
        case AttFormat::Format000_00:
            return "att-%06.2f\r\n";
        case AttFormat::Format00_00:
            return "att-%05.2f\r\n";
        default:
            return "";
        }
}
inline AttFormat stringToFormat(const QString &fmt)
{
    if (fmt == "att-%06.2f\r\n") return AttFormat::Format000_00;
    if (fmt == "att-%05.2f\r\n") return AttFormat::Format00_00;
    return AttFormat::Unknown;
}

// Frequency-banded insertion loss for a digital attenuator. Values are
// already multiplied for cascaded designs (e.g. PE43712 x3 lists 3x the
// single-chip IL). Bands are non-overlapping and ordered by frequency.
struct InsertionLossBand
{
    double freqLowHz;
    double freqHighHz;
    double ilDb;
};

struct DeviceType
{
    QString model;
    double step;
    double max;
    AttFormat format;
    // Conservative absolute-max CW input rating. Vendor markings are scratched
    // and no datasheet ships with these boards, so values are derived from the
    // typical chip behind each variant. Better to warn unnecessarily than fry.
    double maxInputDbm;
    QString chip;
    QList<InsertionLossBand> ilBands;
};

// PE43712 single-chip IL (datasheet typicals, rounded up). Multiplied
// in-place for cascaded models below.
inline QList<InsertionLossBand> pe43712Bands(int cascadeCount)
{
    return {
        { 9.0e3,   1.0e9, 1.0 * cascadeCount },
        { 1.0e9,   2.2e9, 1.2 * cascadeCount },
        { 2.2e9,   4.0e9, 1.4 * cascadeCount },
        { 4.0e9,   6.0e9, 2.0 * cascadeCount }
    };
}

// the key for device identification is max value
// the order is important
// so they probed from the max until succefull read
inline const QList<DeviceType> &deviceTypesTable()
{
    static const QList<DeviceType> table = {
        { "DC-6GHZ-120DB",       0.25, 124.75,  AttFormat::Format000_00, 20.0, "PE43712 x4",
          pe43712Bands(4) },
        { "DC-6GHZ-90DB-V3",     0.25, 95.25,   AttFormat::Format000_00, 20.0, "PE43712 x3",
          pe43712Bands(3) },
        { "DC-3G-90DB-V2",       0.5,  94.5,    AttFormat::Format000_00, 20.0, "HMC624 / DAT-31R5A",
          { { 9.0e3, 3.0e9, 4.8 } } },
        { "DC-6GHZ-30DB",        0.25, 31.75,   AttFormat::Format00_00,  20.0, "PE43712",
          pe43712Bands(1) },
        { "DC-8GHZ-30DB-0.1DB",  0.1,  30.0,    AttFormat::Format00_00,  25.0, "PE43508 / HMC1019",
          { { 9.0e3, 1.0e9, 1.5 },
            { 1.0e9, 3.0e9, 1.7 },
            { 3.0e9, 6.0e9, 2.0 },
            { 6.0e9, 8.0e9, 2.5 } } }
    };
    return table;
}

// Fallback for boards that fail identification: assume "most common 6 GHz
// board" IL plus 1 W rating, per phase 6 plan.
inline QList<InsertionLossBand> fallbackIlBands() { return pe43712Bands(1); }
constexpr double kFallbackMaxInputDbm = 30.0; // 1 W

class AttDevice : public SerialPortInterface
{
    Q_OBJECT

    Q_PROPERTY(QString model      READ model      WRITE setModel      NOTIFY modelChanged)
    Q_PROPERTY(double  step       READ step       WRITE setStep       NOTIFY stepChanged)
    Q_PROPERTY(double  max        READ max        WRITE setMax        NOTIFY maxChanged)
    Q_PROPERTY(QString format     READ format     WRITE setFormat     NOTIFY formatChanged)
    Q_PROPERTY(double currentValue   READ currentValue                  NOTIFY currentValueChanged)
    Q_PROPERTY(double expectedValue  READ expectedValue WRITE setExpectedValue NOTIFY expectedValueChanged)

public:
    explicit AttDevice(QObject *parent = nullptr);

    const QString &model() const { return m_model; }
    void setModel(const QString &model)
    {
        m_model = model;
        emit modelChanged(m_model);
    }

    double step() const { return m_step; }
    void setStep(double step)
    {
        if (qFuzzyCompare(m_step, step))
            return;
        m_step = step;
        emit stepChanged(m_step);
    }

    double max() const { return m_max; }
    void setMax(double max)
    {
        if (qFuzzyCompare(m_max, max))
            return;
        m_max = max;
        emit maxChanged(m_max);
    }

    QString format() const { return formatToString(m_format); }
    void setFormat(const QString &fmt)
    {
        AttFormat newFmt = stringToFormat(fmt);
        if (m_format == newFmt) return;
        m_format = newFmt;
        emit formatChanged(formatToString(m_format));
    }

    double currentValue() const { return m_currentValue; }
    void setCurrentValue(double value)
    {
        if (!qFuzzyCompare(m_currentValue, value))
            {
                m_currentValue = value;
                emit currentValueChanged(value);
            }
    }

    double expectedValue() const { return m_expectedValue; }
    void setExpectedValue(double value)
    {
        if (qFuzzyCompare(m_expectedValue, value)) return;
        m_expectedValue = value;
        emit expectedValueChanged(value);
    }

    double maxInputDbm() const { return m_maxInputDbm; }
    const QString &chip() const { return m_chip; }
    const QList<InsertionLossBand> &ilBands() const { return m_currentIlBands; }

    // Returns the IL for the band that contains freqHz, or 0.0 if freqHz is
    // outside every band (the caller decides how to render "out of band").
    double insertionLossAt(double freqHz) const;

    Q_INVOKABLE void probeDeviceType();
    Q_INVOKABLE void readValue();
    Q_INVOKABLE void writeValue(double value);
    Q_INVOKABLE void setPollingEnabled(bool enabled);

signals:
    void modelChanged(const QString &model);
    void stepChanged(double step);
    void maxChanged(double max);
    void formatChanged(const QString &format);
    void currentValueChanged(double value);
    void expectedValueChanged(double value);
    void valueMatched();
    void valueMismatched(double expected, double actual);
    void valueSetStatus(bool status);
    void detectedDevice(const QString &model, double step, double max, const QString &format,
                        double maxInputDbm, const QString &chip);
    void unknownDevice();

private slots:
    void onSerialPortNewData(const QString &line);
    void onExpectedValueChanged(double value);
    void onDevicePort_started();
    void onProbeTimeout();
    void tryUnknownFormat();
    void onPortClosed();

private:
    enum ProbeState
    {
        ProbeIdle,
        ProbeWaitingSetOK,
        ProbeWaitingValue
    };

    void startProbe();
    void tryCurrentProbe();
    void finishProbe(bool found);

    QString m_model;
    double  m_step         = 0.05;
    double  m_max          = 100.0;
    AttFormat m_format = AttFormat::Format00_00;
    double  m_currentValue = 0.0;
    double  m_expectedValue = 0.0;
    double  m_maxInputDbm = std::numeric_limits<double>::quiet_NaN();
    QString m_chip;
    QList<InsertionLossBand> m_currentIlBands;

    int     m_probeTypeIdx = 0;
    double  m_probeValue   = 0.0;
    ProbeState m_probeState = ProbeIdle;
    QTimer  m_probeTimer;
    bool    m_inProbe      = false;
    int m_unknownFormatIdx = 0;
    QTimer m_unknownFormatTimer;
    bool    m_isProbingUnknownFormats = false;
    QTimer* m_pollingTimer;
    // Per-instance accumulator for partial serial frames. Was a function-
    // static QString in onSerialPortNewData() which silently shared state
    // across every AttDevice instance and survived disconnect/reconnect,
    // mirroring the bug previously fixed in RfpmV7Device. The regression
    // test bufferIsPerInstance_noCrossInstanceLeak in tests/attenuator_chain
    // (or its sibling) pins this.
    QString m_buffer;
};

#endif // ATTDEVICE_H
