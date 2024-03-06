#include "serialportreader.h"
#include "qserialportinfo.h"
#include <QDebug>

SerialPortReader::SerialPortReader(QObject *parent) : QObject{parent}
{


    m_serialPort = new QSerialPort();
    connect(m_serialPort, &QSerialPort::readyRead, this,
            &SerialPortReader::readData);
    connect(m_serialPort, &QSerialPort::errorOccurred, this,
            &SerialPortReader::serialError);
}

SerialPortReader::~SerialPortReader()
{
    stopPort();
    m_serialPort->deleteLater();
}

void SerialPortReader::onPortName_changed() {}

void SerialPortReader::onBaudRate_changed() {}

void SerialPortReader::startPort()
{
    qDebug() << "startPort()" << getportName() << getbaudRate();
    stopPort();
    m_serialPort->setPortName(getportName());
    m_serialPort->setBaudRate(getbaudRate());
    if (!m_serialPort->open(QIODevice::ReadOnly))
    {
        qDebug() << "failed to open";
    }
}

void SerialPortReader::stopPort()
{
    if (m_serialPort->isOpen())
    {
        qDebug() << "close current port";
        m_serialPort->close();
    }
}

void SerialPortReader::onPort_started() {}

void SerialPortReader::onPort_stopped() {}

void SerialPortReader::readData()
{
    qDebug() << "readData()";
    QByteArray data = m_serialPort->readAll();
    qDebug()<<data;
    //emit serialPortNewData(QString(data));
    QStringList wholeStringList;
    QRegularExpression reg("[$].+?[$]");

    QRegularExpressionMatchIterator i = reg.globalMatch(data);
    if (i.isValid())
    {
        while (i.hasNext())
        {
            QRegularExpressionMatch match = i.next();
            qDebug()<<match.captured(0);
            emit serialPortNewData(QString(match.captured(0).trimmed()));
        }

    }
}

void SerialPortReader::serialError(
    QSerialPort::SerialPortError serialPortError)
{
    qDebug() << "serialError";
    if (serialPortError == QSerialPort::ReadError)
    {
        emit serialPortErrorSignal("Error " + m_serialPort->portName() + " " +
                                   m_serialPort->errorString());
        qDebug()<<m_serialPort->portName()<<m_serialPort->errorString();
    }
}
