#include "qtdigitalattenuator.h"
#include "ui_qtdigitalattenuator.h"

QtDigitalAttenuator::QtDigitalAttenuator(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::QtDigitalAttenuator)
{
    ui->setupUi(this);
    ui->currentattenuation_lcdNumber->setSegmentStyle(QLCDNumber::Flat );
    ondeviceConsole_pushButton_clicked();
    ui->currentattenuation_lcdNumber->setStyleSheet("QLCDNumber{ background-color: green; color: yellow;}");
    attenuation_doubleSpinBox_debounceTimer = new QTimer(this);
    attenuation_doubleSpinBox_debounceTimer->setSingleShot(true);
    connect(attenuation_doubleSpinBox_debounceTimer, &QTimer::timeout, this, [this]() {
        if(ui->autoset_checkBox->isChecked())
        {
            serialAttenuator->writeValue(ui->attenuation_doubleSpinBox->value());
        }
    });
    updateDeviceList();
    ui->statusSet_label->setAlignment(Qt::AlignCenter);
    ui->refreshDevices_toolbutton->setIcon(QIcon::fromTheme("view-refresh",QIcon(":/images/view-refresh.svg")));
    serialAttenuator= new AttDevice();
    Q_EMIT(ondevice_comboBox_currentIndexChanged());
    connect(ui->device_comboBox, &QComboBox::currentTextChanged, this, &QtDigitalAttenuator::ondevice_comboBox_currentIndexChanged);
    connect(serialAttenuator,&AttDevice::serialPortNewData,this,&QtDigitalAttenuator::updateData);
    connect(serialAttenuator,&AttDevice::serialPortErrorSignal,this,&QtDigitalAttenuator::on_serialPortError);
    connect(serialAttenuator,&AttDevice::currentValueChanged,this,&QtDigitalAttenuator::on_currentAttenuation_changed);
    connect(serialAttenuator,&AttDevice::detectedDevice,this,&QtDigitalAttenuator::ondetectedDevice);
    connect(ui->attenuation_doubleSpinBox,&QDoubleSpinBox::valueChanged,this,&QtDigitalAttenuator::onattenuation_doubleSpinBox_valueChanged);
    connect(ui->deviceConsole_pushButton,&QPushButton::clicked,this,&QtDigitalAttenuator::ondeviceConsole_pushButton_clicked);
    connect(serialAttenuator,&AttDevice::valueSetStatus,this,&QtDigitalAttenuator::ondeviceSetStatus);
}

QtDigitalAttenuator::~QtDigitalAttenuator()
{
    serialAttenuator->disconnect();
    serialAttenuator->deleteLater();
    delete ui;
}

void QtDigitalAttenuator::ondevice_comboBox_currentIndexChanged()
{
    qDebug()<<"ondevice_comboBox_currentIndexChanged"<<ui->device_comboBox->currentData().toString();
    if(ui->device_comboBox->currentText().isEmpty())
    {
        ui->connect_pushButton->setDisabled(true);
        ui->disconnect_pushButton->setDisabled(true);
    }
    else
    {
        ui->connect_pushButton->setDisabled(false);
        ui->disconnect_pushButton->setDisabled(false);
    }
}

void QtDigitalAttenuator::updateData(const QString &data)
{
    qDebug()<<"updateData"<<data;
    QString curdate=QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz");
    ui->data_plainTextEdit->appendPlainText(curdate+" "+ui->device_comboBox->currentData().toString()+" "+data);
}

void QtDigitalAttenuator::on_connect_pushButton_clicked()
{
    qDebug()<<"on_connect_pushButton_clicked";

    if(!ui->device_comboBox->currentData().toString().isEmpty())
    {
        serialAttenuator->setportName(ui->device_comboBox->currentData().toString());
        serialAttenuator->setbaudRate(115200);
        serialAttenuator->startPort();

        //Q_EMIT(on_set_pushButton_clicked());
       updateDeviceList();
    }

}

void QtDigitalAttenuator::on_disconnect_pushButton_clicked()
{
    qDebug()<<"on_disconnect_pushButton_clicked";
    serialAttenuator->stopPort();
    ui->model_lineEdit->setText("");
    emit modelChanged(tr("Disconnected"));
    updateDeviceList();

}

void QtDigitalAttenuator::on_refreshDevices_toolbutton_clicked()
{
    qDebug()<<"on_refreshDevices_toolbutton_clicked";
    updateDeviceList();
}

void QtDigitalAttenuator::updateDeviceList()
{
    qDebug()<<"updateDeviceList";
    ui->device_comboBox->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos)
    {

        if(info.hasVendorIdentifier() && QString::number(info.vendorIdentifier(),16)=="483")
        {
            qDebug()<<info.hasVendorIdentifier() <<QString::number(info.vendorIdentifier());
            QString busyText;
            QSerialPort serialPort(info);
            if (!serialPort.open(QIODevice::ReadWrite)) {
                busyText = tr(" [Busy]");
            } else {
                serialPort.close();
                busyText = "           ";
            }
            QString s = info.portName() +
                        tr(" ") + info.manufacturer() +
                        tr(" ") + info.serialNumber() +
                        tr(" (") + (info.hasVendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : QString()) +
                        tr(":") + (info.hasProductIdentifier() ? QString::number(info.productIdentifier(), 16) : QString()) +")"+
                        busyText;
            qInfo()<<info.portName();

            qInfo()<<s;
            ui->device_comboBox->addItem(s,info.portName());
        }
    }
}

void QtDigitalAttenuator::on_serialPortError(QString error)
{
    qDebug()<<"on_serialPortError";
    ui->statusbar->showMessage(error,1000);
    Q_EMIT(ondevice_comboBox_currentIndexChanged());
}

void QtDigitalAttenuator::on_send_pushButton_clicked()
{
    qDebug()<<"on_set_pushButton_clicked";
    serialAttenuator->writeData((ui->write_data->text()+QString("\r\n")).toLatin1());
}

void QtDigitalAttenuator::on_set_pushButton_clicked()
{
    qDebug()<<"on_set_pushButton_clicked";
    qDebug()<<ui->attenuation_doubleSpinBox->value();
    serialAttenuator->writeValue(ui->attenuation_doubleSpinBox->value());
}


void QtDigitalAttenuator::on_currentAttenuation_changed(double value)
{
    qDebug()<<Q_FUNC_INFO<<value;
    ui->currentattenuation_lcdNumber->display(value);

    emit currentValueChanged(value);
}

void QtDigitalAttenuator::onattenuation_doubleSpinBox_valueChanged(double value)
{
    qDebug()<<Q_FUNC_INFO<<value;
    attenuation_doubleSpinBox_debounceTimer->start(400);
}

void QtDigitalAttenuator::ondetectedDevice(const QString &model, double step, double max, const QString &format)
{
    qDebug()<<Q_FUNC_INFO<<model<<step<<max<<format;
    ui->model_lineEdit->setText(model);
    emit modelChanged(model);
    disconnect(ui->attenuation_doubleSpinBox,&QDoubleSpinBox::valueChanged,this,&QtDigitalAttenuator::onattenuation_doubleSpinBox_valueChanged);
    ui->attenuation_doubleSpinBox->setValue(serialAttenuator->currentValue());
    connect(ui->attenuation_doubleSpinBox,&QDoubleSpinBox::valueChanged,this,&QtDigitalAttenuator::onattenuation_doubleSpinBox_valueChanged);
}

void QtDigitalAttenuator::ondeviceConsole_pushButton_clicked()
{
    qDebug()<<Q_FUNC_INFO;
    if(ui->deviceConsole_pushButton->isChecked())
    {
        ui->deviceConsole_groupBox->setVisible(true);
        ui->deviceConsole_pushButton->setText(tr("Hide Device Console"));
    }
    else
    {
        ui->deviceConsole_groupBox->setVisible(false);
        ui->deviceConsole_pushButton->setText(tr("Show Device Console"));
    }
}

void QtDigitalAttenuator::ondeviceSetStatus(bool status)
{
    qDebug()<<Q_FUNC_INFO<<status;
    if(status)
    {
        ui->statusSet_label->setText("OK");
        ui->statusSet_label->setStyleSheet("QLabel { background-color: #4CAF50; color: white; border-radius: 10px; padding: 3px 9px; font-weight: bold; }");
    }
    else
    {
        ui->statusSet_label->setText("ERROR");
        ui->statusSet_label->setStyleSheet("QLabel { background-color: #F44336; color: white; border-radius: 10px; padding: 3px 9px; font-weight: bold; }");
    }
    emit valueSetStatus(status);
}
