#include "serialportinterface.h"
#include "qserialportinfo.h"

SerialPortInterface::SerialPortInterface(QObject *parent) : QObject{parent}
{


    m_serialPort = new QSerialPort();
    connect(m_serialPort, &QSerialPort::readyRead, this,
            &SerialPortInterface::readData);
    connect(m_serialPort, &QSerialPort::errorOccurred, this,
            &SerialPortInterface::serialError);
}

SerialPortInterface::~SerialPortInterface()
{
    stopPort();
    m_serialPort->deleteLater();
}

void SerialPortInterface::onPortName_changed() {}

void SerialPortInterface::onBaudRate_changed() {}

void SerialPortInterface::startPort()
{
    qDebug()<<"startPort()"<<getportName()<<getbaudRate();
    stopPort();
    m_serialPort->setPortName(getportName());
    m_serialPort->setBaudRate(getbaudRate());
    if (!m_serialPort->open(QIODevice::ReadWrite))
    {
        qDebug()<<"failed to open";
        emit serialPortErrorSignal("Failed to open " + getportName());
    }
    else
    {
        emit portOpened();
    }
}

void SerialPortInterface::stopPort()
{
    if (m_serialPort->isOpen())
    {
        qDebug()<<"close current port";
        m_serialPort->close();
        emit portClosed();
    }
}

void SerialPortInterface::onPort_started() {}

void SerialPortInterface::onPort_stopped() {}

void SerialPortInterface::readData()
{
    qDebug()<<"readData()";
    QByteArray data = m_serialPort->readAll();
    qDebug()<<data;
    emit serialPortNewData(QString(data));
    QRegularExpression reg("[$].+?[$]");

    QRegularExpressionMatchIterator i = reg.globalMatch(data);
    if (i.isValid())
    {
        while (i.hasNext())
        {
            QRegularExpressionMatch match = i.next();
            qDebug()<<match.captured(0);
            emit serialPortNewRFData(QString(match.captured(0).trimmed()));
        }

    }
}

void SerialPortInterface::writeData(const QByteArray &data)
{
    if(!m_serialPort->isOpen()) {
        qWarning()<<"Port not open. Skipping write.";
        return;
    }
    qDebug()<<"writedata: "<<data;
    m_serialPort->write(data);
}

void SerialPortInterface::serialError(
    QSerialPort::SerialPortError serialPortError)
{
    qDebug()<<"serialError";
    if (serialPortError != QSerialPort::NoError)
    {
        emit serialPortErrorSignal("Error " + m_serialPort->portName() + " " +
                                   m_serialPort->errorString());
        qDebug()<<m_serialPort->portName()<<m_serialPort->errorString();
        stopPort();
    }
}
