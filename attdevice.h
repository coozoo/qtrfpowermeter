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

struct DeviceType
{
    QString model;
    double step;
    double max;
    AttFormat format;
};

// the key for device identification is max value
// the order is important
// so they probed from the max until succefull read
static const DeviceType deviceTypes[] =
{
    { "DC-6GHZ-120DB",       0.25, 124.75,  AttFormat::Format000_00 },
    { "DC-6GHZ-90DB-V3",     0.25, 95.25,   AttFormat::Format000_00 },
    { "DC-3G-90DB-V2",       0.5,  94.5,    AttFormat::Format000_00 },
    { "DC-6GHZ-30DB",        0.25, 31.75,   AttFormat::Format00_00 },
    { "DC-8GHZ-30DB-0.1DB",  0.1,  30.0,    AttFormat::Format00_00 }
};

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

    Q_INVOKABLE void probeDeviceType();
    Q_INVOKABLE void readValue();
    Q_INVOKABLE void writeValue(double value);

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
    void detectedDevice(const QString &model, double step, double max, const QString &format);
    void unknownDevice();

private slots:
    void onSerialPortNewData(const QString &line);
    void onExpectedValueChanged(double value);
    void onDevicePort_started();
    void onProbeTimeout();
    void tryUnknownFormat();

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

    int     m_probeTypeIdx = 0;
    double  m_probeValue   = 0.0;
    ProbeState m_probeState = ProbeIdle;
    QTimer  m_probeTimer;
    bool    m_inProbe      = false;
    int m_unknownFormatIdx = 0;
    QTimer m_unknownFormatTimer;
    bool    m_isProbingUnknownFormats = false;
};

#endif // ATTDEVICE_H
