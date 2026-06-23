#ifndef SERIALPORTINTERFACE_H
#define SERIALPORTINTERFACE_H

#include <QSerialPort>
#include <QObject>
#include <QWidget>
#include <QRegularExpression>
#include <QDebug>

class SerialPortInterface : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString portName
                   READ getportName
                       WRITE setportName
               )
    Q_PROPERTY(int baudRate
                   READ getbaudRate
                       WRITE setbaudRate
               )
    // Opt-in raw mode. When enabled, readData() / processIncomingBytes()
    // emit ONLY serialPortNewRawData() and skip the legacy
    // line/binary/RF-regex split. Default off so every existing device
    // driver keeps its old wiring. TinySa flips this on at construction.
    Q_PROPERTY(bool rawMode READ getRawMode WRITE setRawMode)
public:
    explicit SerialPortInterface(QObject *parent = nullptr);
    ~SerialPortInterface();

    void setportName(const QString &m_portName)
    {
        portName = m_portName;
        //emit portName_changed();
    }
    const QString &getportName() const
    {
        return portName;
    }


    void setbaudRate(int m_baudRate)
    {
        baudRate = m_baudRate;
        //emit baudRate_changed();
    }
    int getbaudRate() const
    {
        return baudRate;
    }

    bool isPortOpen() const;

    void setRawMode(bool mode) { m_rawMode = mode; }
    bool getRawMode() const { return m_rawMode; }

    // Drop bytes sitting in the input buffer. Used by TinySa between
    // handshake retries; legacy devices have no reason to call it.
    void clearInputBuffer();


public slots:
    void startPort();
    void stopPort();
    void writeData(const QByteArray &data);

    // Emit the data-arrival signals for a chunk of bytes. readData()
    // delegates to this so tests can verify the emit wiring without
    // opening a real port.
    void processIncomingBytes(const QByteArray &data);

private:
    QString portName;
    int baudRate;
    QSerialPort *m_serialPort = nullptr;
    QByteArray m_readData;
    QByteArray key16;
    bool m_rawMode = false;


signals:
    void serialPortNewData(QString line);
    void serialPortNewRFData(QString line);
    void serialPortNewBinaryData(const QByteArray &data);
    void serialPortNewRawData(const QByteArray &data); // TinySa raw stream
    void serialLoRaAppMessage(QByteArray header, QString line);
    void portName_changed();
    void baudRate_changed();
    void serialPortErrorSignal(QString error);
    void portOpened();
    void portClosed();

private slots:
    void onPortName_changed();
    void onBaudRate_changed();
    void onPort_started();
    void onPort_stopped();
    void readData();
    void serialError(QSerialPort::SerialPortError serialPortError);
};

#endif // SERIALPORTINTERFACE_H
