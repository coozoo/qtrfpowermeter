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

    ui->useHardButtons_checkBox->setToolTip(tr("Periodically read the attenuation value from the device every %1 ms.").arg(hardwareReadIntervalMs));

    updateDeviceList();
    ui->statusSet_label->setAlignment(Qt::AlignCenter);
    ui->refreshDevices_toolbutton->setIcon(QIcon::fromTheme("view-refresh",QIcon(":/images/view-refresh.svg")));
    serialAttenuator= new AttDevice();
    Q_EMIT(ondevice_comboBox_currentIndexChanged());
    connect(ui->device_comboBox, &QComboBox::currentIndexChanged, this, &QtDigitalAttenuator::ondevice_comboBox_currentIndexChanged);
    connect(serialAttenuator,&AttDevice::serialPortNewData,this,&QtDigitalAttenuator::updateData);
    connect(serialAttenuator,&AttDevice::serialPortErrorSignal,this,&QtDigitalAttenuator::on_serialPortError);
    connect(serialAttenuator,&AttDevice::currentValueChanged,this,&QtDigitalAttenuator::on_currentAttenuation_changed);
    connect(serialAttenuator, &AttDevice::portOpened, this, &QtDigitalAttenuator::onPortOpened);
    connect(serialAttenuator, &AttDevice::portClosed, this, &QtDigitalAttenuator::onPortClosed);
    connect(this, &QtDigitalAttenuator::isConnectedChanged, this, &QtDigitalAttenuator::onIsConnectedChanged);
    connect(serialAttenuator,&AttDevice::detectedDevice,this,&QtDigitalAttenuator::ondetectedDevice);
    connect(ui->attenuation_doubleSpinBox,&QDoubleSpinBox::valueChanged,this,&QtDigitalAttenuator::onattenuation_doubleSpinBox_valueChanged);
    connect(ui->deviceConsole_pushButton,&QPushButton::clicked,this,&QtDigitalAttenuator::ondeviceConsole_pushButton_clicked);
    connect(serialAttenuator,&AttDevice::valueSetStatus,this,&QtDigitalAttenuator::ondeviceSetStatus);
    connect(ui->useHardButtons_checkBox, &QCheckBox::stateChanged, this, &QtDigitalAttenuator::on_useHardButtons_checkBox_stateChanged);
}

QtDigitalAttenuator::~QtDigitalAttenuator()
{
    serialAttenuator->disconnect();
    serialAttenuator->deleteLater();
    delete ui;
}



void QtDigitalAttenuator::onPortOpened()
{
    qDebug() << Q_FUNC_INFO;
    setDeviceError("");
    setIsConnected(true);
    updateDeviceList();
}

void QtDigitalAttenuator::onPortClosed()
{
    qDebug() << Q_FUNC_INFO;
    ui->model_lineEdit->setText("");
    emit modelChanged(tr("Disconnected"));
    setIsConnected(false);
    updateDeviceList();
    ui->statusSet_label->setText("----");
    serialAttenuator->setCurrentValue(0);
}

void QtDigitalAttenuator::ondevice_comboBox_currentIndexChanged()
{
    qDebug()<<"ondevice_comboBox_currentIndexChanged";
    onIsConnectedChanged(isConnected());
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

    if (ui->device_comboBox->currentIndex() == -1)
    {
        qDebug() << "Connect clicked with no device selected.";
        return;
    }
    // This is the commit point. Set the current device from the UI selection.
    QString selectedPort = ui->device_comboBox->currentData(PortNameRole).toString();
    QString selectedSerial = ui->device_comboBox->currentData(SerialNumberRole).toString();

    setCurrentDevice(selectedSerial);
    qDebug() << "Attempting to connect to" << selectedPort << "with S/N" << currentDevice();

    if(!selectedPort.isEmpty())
    {
        serialAttenuator->setportName(selectedPort);
        serialAttenuator->setbaudRate(115200);
        serialAttenuator->startPort();
    }
}

void QtDigitalAttenuator::on_disconnect_pushButton_clicked()
{
    qDebug()<<"on_disconnect_pushButton_clicked";
    serialAttenuator->stopPort();
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
    int newIndexToSelect = -1;
    for (const QSerialPortInfo &info : infos)
    {
        if(info.hasVendorIdentifier() && QString::number(info.vendorIdentifier(),16)=="483")
        {
            qDebug()<<info.hasVendorIdentifier() <<QString::number(info.vendorIdentifier());
            bool isBusy = false;
            QString busyText;
            QSerialPort serialPort(info);
            if (!serialPort.open(QIODevice::ReadWrite)) {
                busyText = tr(" [Busy]");
                isBusy = true;
            } else {
                serialPort.close();
                busyText = "           ";
            }
            QString s = info.portName() +
                        " " + info.manufacturer() +
                        " " + info.serialNumber() +
                        " (" + (info.hasVendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : QString()) +
                        ":" + (info.hasProductIdentifier() ? QString::number(info.productIdentifier(), 16) : QString()) +")"+
                        busyText;
            qInfo() << info.portName();
            qInfo() << s;

            int newIndex = ui->device_comboBox->count();
            ui->device_comboBox->addItem(s, info.portName()); // Default data is port name for logging in updateData
            ui->device_comboBox->setItemData(newIndex, info.portName(), PortNameRole);
            ui->device_comboBox->setItemData(newIndex, info.systemLocation(), SystemLocationRole);
            ui->device_comboBox->setItemData(newIndex, info.description(), DescriptionRole);
            ui->device_comboBox->setItemData(newIndex, info.manufacturer(), ManufacturerRole);
            ui->device_comboBox->setItemData(newIndex, info.serialNumber(), SerialNumberRole);
            ui->device_comboBox->setItemData(newIndex, (info.hasVendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : QString()), VendorIDRole);
            ui->device_comboBox->setItemData(newIndex, (info.hasProductIdentifier() ? QString::number(info.productIdentifier(), 16) : QString()), ProductIDRole);
            ui->device_comboBox->setItemData(newIndex, isBusy, IsBusyRole);

            if (info.serialNumber() == currentDevice())
            {
                newIndexToSelect = newIndex;
            }
        }
    }

    if (newIndexToSelect != -1)
    {
        ui->device_comboBox->setCurrentIndex(newIndexToSelect);
    }
    ondevice_comboBox_currentIndexChanged();
}

void QtDigitalAttenuator::on_serialPortError(const QString &error)
{
    qDebug()<<"on_serialPortError";
    setDeviceError(error);
    setIsConnected(false);
    updateDeviceList();
    ui->statusSet_label->setStyleSheet("QLabel { background-color: #F44336; color: white; border-radius: 10px; padding: 3px 9px; font-weight: bold; }");
    ui->statusSet_label->setText("----");
    serialAttenuator->setCurrentValue(0);
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

    if (ui->useHardButtons_checkBox->isChecked()) {
        ui->attenuation_doubleSpinBox->blockSignals(true);
        ui->attenuation_doubleSpinBox->setValue(value);
        ui->attenuation_doubleSpinBox->blockSignals(false);
    }

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

void QtDigitalAttenuator::onIsConnectedChanged(bool connected)
{
    qDebug() << Q_FUNC_INFO << "Connected:" << connected;
    ui->device_comboBox->setEnabled(!connected);
    ui->disconnect_pushButton->setEnabled(connected);

    if (ui->device_comboBox->currentIndex() == -1)
    {
        ui->connect_pushButton->setEnabled(false);
    }
    else
    {
        ui->connect_pushButton->setEnabled(!connected && !ui->device_comboBox->currentData(IsBusyRole).toBool());
    }

    if (connected)
    {
        if (ui->device_comboBox->currentIndex() != -1) {
             ui->statusbar->showMessage(tr("Connected to %1").arg(ui->device_comboBox->currentData(PortNameRole).toString()));
        } else {
             ui->statusbar->showMessage(tr("Connected"));
        }
    }
    else
    {
        if (!deviceError().isEmpty())
        {
            ui->statusbar->showMessage(tr("Error: %1").arg(deviceError()));
        } else {
            ui->statusbar->showMessage(tr("Disconnected"));
        }
    }
}

void QtDigitalAttenuator::on_useHardButtons_checkBox_stateChanged(int state)
{
    bool checked = (state == Qt::Checked);
    ui->attenuation_doubleSpinBox->setEnabled(!checked);
    ui->set_pushButton->setEnabled(!checked);
    ui->autoset_checkBox->setEnabled(!checked);

    serialAttenuator->setPollingEnabled(checked && isConnected());
}
