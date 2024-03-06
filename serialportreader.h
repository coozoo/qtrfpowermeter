#ifndef SERIALPORTREADER_H
#define SERIALPORTREADER_H

#include <QSerialPort>
#include <QObject>
#include <QWidget>
#include <QRegExp>
#include <QRegularExpression>


class SerialPortReader : public QObject
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
public:
    explicit SerialPortReader(QObject *parent = nullptr);
    ~SerialPortReader();

    void setportName(QString m_portName)
    {
        portName = m_portName;
        //emit portName_changed();
    }
    QString getportName() const
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


public slots:
    void startPort();
    void stopPort();

private:
    QString portName;
    int baudRate;
    QSerialPort *m_serialPort = nullptr;
    QByteArray m_readData;
    QByteArray key16;


signals:
    void serialPortNewData(QString line);
    void serialLoRaAppMessage(QByteArray header, QString line);
    void serialLoRaUnknownMessage(QString line);
    void portName_changed();
    void baudRate_changed();
    void serialPortErrorSignal(QString line);

private slots:
    void onPortName_changed();
    void onBaudRate_changed();
    void onPort_started();
    void onPort_stopped();
    void readData();
    void serialError(QSerialPort::SerialPortError serialPortError);
};

#endif // SERIALPORTREADER_H
