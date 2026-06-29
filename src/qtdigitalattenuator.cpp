#include "qtdigitalattenuator.h"
#include "ui_qtdigitalattenuator.h"
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <cmath>

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

    setupInsertionLossWidgets();
}

void QtDigitalAttenuator::setupInsertionLossWidgets()
{
    m_ilGroupBox = new QGroupBox(tr("Insertion Loss"), this);
    m_ilGroupBox->setVisible(false); // shown after a model is detected

    QVBoxLayout *col = new QVBoxLayout(m_ilGroupBox);
    col->setContentsMargins(6, 6, 6, 6);

    m_ilTable = new QTableWidget(m_ilGroupBox);
    m_ilTable->setRowCount(1);
    m_ilTable->setColumnCount(0);
    m_ilTable->setVerticalHeaderLabels({ tr("IL dB") });
    m_ilTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_ilTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ilTable->setFocusPolicy(Qt::NoFocus);
    m_ilTable->setFixedHeight(56);
    m_ilTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_ilTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    m_effectiveLabel = new QLabel(tr("Effective: --- dB"), m_ilGroupBox);
    m_effectiveLabel->setStyleSheet("font-weight: bold;");
    m_effectiveLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_effectiveLabel->setToolTip(tr("Sum of nominal attenuation and the insertion loss for the current operating band."));

    col->addWidget(m_ilTable);
    col->addWidget(m_effectiveLabel);

    // Insert above the device-console button (row 4 of ui->gridLayout).
    ui->gridLayout->addWidget(m_ilGroupBox, 3, 1);
}

static QString formatBandLabel(double lowHz, double highHz)
{
    // Compact form: no space between digits and unit, no space around the
    // range separator, and if both ends share a unit drop it from the low
    // end ("4-6GHz" instead of "4GHz-6GHz").
    auto split = [](double hz) -> std::pair<QString, QString> {
        if (hz >= 1.0e9) return { QString::number(hz / 1.0e9, 'g', 3), QStringLiteral("GHz") };
        if (hz >= 1.0e6) return { QString::number(hz / 1.0e6, 'g', 3), QStringLiteral("MHz") };
        if (hz >= 1.0e3) return { QString::number(hz / 1.0e3, 'g', 3), QStringLiteral("kHz") };
        return { QString::number(hz, 'g', 3), QStringLiteral("Hz") };
    };
    auto lo = split(lowHz);
    auto hi = split(highHz);
    if (lo.second == hi.second)
        return lo.first + QStringLiteral("-") + hi.first + hi.second;
    return lo.first + lo.second + QStringLiteral("-") + hi.first + hi.second;
}

void QtDigitalAttenuator::rebuildIlTableForBands(const QList<InsertionLossBand> &bands)
{
    if (!m_ilTable) return;
    m_ilTable->clearContents();
    m_ilTable->setColumnCount(bands.size());
    QStringList headers;
    headers.reserve(bands.size());
    for (int i = 0; i < bands.size(); ++i)
        {
            const InsertionLossBand &b = bands.at(i);
            headers << formatBandLabel(b.freqLowHz, b.freqHighHz);
            QTableWidgetItem *item = new QTableWidgetItem(QString::number(b.ilDb, 'f', 2));
            item->setTextAlignment(Qt::AlignCenter);
            m_ilTable->setItem(0, i, item);
        }
    m_ilTable->setHorizontalHeaderLabels(headers);
    m_ilGroupBox->setVisible(!bands.isEmpty());
    refreshIlHighlight();
}

void QtDigitalAttenuator::refreshIlHighlight()
{
    if (!m_ilTable || !serialAttenuator) return;
    const auto &bands = serialAttenuator->ilBands();
    int activeCol = -1;
    if (!std::isnan(m_currentFreqHz))
        {
            for (int i = 0; i < bands.size(); ++i)
                {
                    const auto &b = bands.at(i);
                    if (m_currentFreqHz >= b.freqLowHz && m_currentFreqHz < b.freqHighHz)
                        {
                            activeCol = i;
                            break;
                        }
                }
        }
    for (int i = 0; i < m_ilTable->columnCount(); ++i)
        {
            QTableWidgetItem *cell = m_ilTable->item(0, i);
            if (!cell) continue;
            cell->setBackground(i == activeCol ? QBrush(QColor("#FFF3B0")) : QBrush());
            cell->setForeground(i == activeCol ? QBrush(QColor("#222222")) : QBrush());
        }
    if (activeCol >= 0)
        {
            m_currentIlDb = bands.at(activeCol).ilDb;
        }
    else
        {
            m_currentIlDb = 0.0;
        }
}

void QtDigitalAttenuator::setCurrentFrequencyHz(double freqHz)
{
    if (qFuzzyCompare(m_currentFreqHz + 1.0, freqHz + 1.0)) return;
    m_currentFreqHz = freqHz;
    refreshIlHighlight();
    emitEffective();
}

void QtDigitalAttenuator::emitEffective()
{
    if (!serialAttenuator) return;
    const double nominal = serialAttenuator->currentValue();
    const double il = m_currentIlDb;
    const double effective = nominal + il;
    if (m_effectiveLabel)
        {
            if (std::isnan(m_currentFreqHz))
                {
                    m_effectiveLabel->setText(tr("Effective: --- dB (set frequency)"));
                }
            else if (m_ilTable && m_ilTable->columnCount() > 0 && il == 0.0)
                {
                    m_effectiveLabel->setText(tr("Effective: %1 dB (out of band)").arg(effective, 0, 'f', 2));
                }
            else
                {
                    m_effectiveLabel->setText(tr("Effective: %1 dB  (= %2 + IL %3)")
                                              .arg(effective, 0, 'f', 2)
                                              .arg(nominal, 0, 'f', 2)
                                              .arg(il, 0, 'f', 2));
                }
        }
    emit effectiveAttenuationChanged(nominal, il, effective);
}

QtDigitalAttenuator::~QtDigitalAttenuator()
{
    serialAttenuator->disconnect();
    serialAttenuator->deleteLater();
    delete ui;
}



void QtDigitalAttenuator::onPortOpened()
{
    qDebug()<<Q_FUNC_INFO;
    setDeviceError("");
    setIsConnected(true);
    updateDeviceList();
}

void QtDigitalAttenuator::onPortClosed()
{
    qDebug()<<Q_FUNC_INFO;
    ui->model_lineEdit->setText("");
    emit modelChanged(tr("Disconnected"));
    setIsConnected(false);
    updateDeviceList();
    ui->statusSet_label->setText("----");
    if (m_ilGroupBox) m_ilGroupBox->setVisible(false);
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
        qDebug()<<"Connect clicked with no device selected.";
        return;
    }
    // This is the commit point. Set the current device from the UI selection.
    QString selectedPort = ui->device_comboBox->currentData(PortNameRole).toString();
    QString selectedSerial = ui->device_comboBox->currentData(SerialNumberRole).toString();

    setCurrentDevice(selectedSerial);
    qDebug()<<"Attempting to connect to"<<selectedPort<<"with S/N"<<currentDevice();

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
            // TinySa shares the STM VID 0x0483 with the digital attenuator boards.
            // Skip it here so it does not appear in the attenuator picker; the
            // calibration panel's own picker handles TinySa. Mirrors the same
            // guard in MainWindow::updateDeviceList for the power-meter combo.
            if (info.description().contains(QStringLiteral("tinysa"), Qt::CaseInsensitive))
                continue;

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
            qInfo()<<info.portName();
            qInfo()<<s;

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
    emitEffective();
}

void QtDigitalAttenuator::onattenuation_doubleSpinBox_valueChanged(double value)
{
    qDebug()<<Q_FUNC_INFO<<value;
    attenuation_doubleSpinBox_debounceTimer->start(400);
}

void QtDigitalAttenuator::ondetectedDevice(const QString &model, double step, double max, const QString &format,
                                           double maxInputDbm, const QString &chip)
{
    qDebug()<<Q_FUNC_INFO<<model<<step<<max<<format<<maxInputDbm<<chip;
    ui->model_lineEdit->setText(model);
    if (!chip.isEmpty() && !std::isnan(maxInputDbm)) {
        ui->model_lineEdit->setToolTip(tr("Chip: %1\nMax CW input: %2 dBm (conservative)")
                                       .arg(chip).arg(maxInputDbm, 0, 'f', 1));
    } else {
        ui->model_lineEdit->setToolTip(tr("Unknown board: no rating data."));
    }
    m_maxInputDbm = maxInputDbm;
    m_chip = chip;
    rebuildIlTableForBands(serialAttenuator->ilBands());
    emit modelChanged(model);
    emit maxInputDbmChanged(maxInputDbm, chip);
    disconnect(ui->attenuation_doubleSpinBox,&QDoubleSpinBox::valueChanged,this,&QtDigitalAttenuator::onattenuation_doubleSpinBox_valueChanged);
    ui->attenuation_doubleSpinBox->setValue(serialAttenuator->currentValue());
    connect(ui->attenuation_doubleSpinBox,&QDoubleSpinBox::valueChanged,this,&QtDigitalAttenuator::onattenuation_doubleSpinBox_valueChanged);
    emitEffective();
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
    qDebug()<<Q_FUNC_INFO<<"Connected:"<<connected;
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
