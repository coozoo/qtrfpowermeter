#ifndef RFPMV5DEVICE_H
#define RFPMV5DEVICE_H

#include "abstractpmdevice.h"
#include <QTimer>
#include <QElapsedTimer>

class RfpmV5Device : public AbstractPMDevice
{
    Q_OBJECT

public:
    explicit RfpmV5Device(const PMDeviceProperties &props, QObject *parent = nullptr);
    virtual ~RfpmV5Device();

    Q_INVOKABLE void connectDevice(const QString &portName) override;
    Q_INVOKABLE void disconnectDevice() override;
    Q_INVOKABLE void setFrequency(quint64 freqHz) override;
    Q_INVOKABLE void setOffset(double offsetDb) override;
    void processData(const QString &data) override;

    Q_INVOKABLE void readSettings();
    Q_INVOKABLE void setSampleRate(int rate);

    // V5 streams every parsed +XXXYYYYY[umw] sample via rawSampleReady,
    // so the Fast View can use the un-decimated feed.
    bool emitsRawSampleStream() const override { return true; }
    // Empirical samples-per-second measured over a sliding window.
    // Used by FastView for its X-axis sample-domain spacing.
    int  currentSamplingRateHz() const override { return m_rawSampleRateHz; }

private slots:
    void onSerialPortNewData(const QString &data);
    void onSerialPortError(const QString &error);
    void sendBufferedCommand();
    void onSampleTimerTimeout();
    void onIdentificationTimeout();

private:
    SerialPortInterface *m_serialPort;
    QTimer *m_commandTimer;
    QTimer *m_sampleTimer;
    QTimer *m_readbackTimer;
    QTimer *m_identificationTimer;

    QString m_buffer;

    // Measurement state
    double m_accumulatedDbm;
    int m_sampleCount;
    int m_timerIntervalMs;
    QString m_lastRawPacket;

    // Raw-sample rate tracker. Counts incoming +XXXYYYYY[umw] frames in
    // a 250 ms window and converts to Hz when the window closes; that
    // becomes the value Fast View positions samples by.
    QElapsedTimer m_rateWindow;
    int           m_rateWindowSamples = 0;
    int           m_rawSampleRateHz   = 0;

    // Device state
    quint64 m_currentFrequencyHz;
    double m_currentOffsetDb;
    bool m_isIdentified = false;
};

#endif // RFPMV5DEVICE_H
