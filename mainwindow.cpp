#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    datetimefile = QDateTime::fromMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch()).toString("dd.MM.yyyy_hh.mm.ss.zzz");

#ifdef Q_OS_WIN
    rootstatsdir = qApp->applicationDirPath();
    statsdirlocation = "/stats/";
#endif
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
    rootstatsdir = QDir::home().absolutePath();
    statsdirlocation = "/rfpowermeter/stats/";
#endif

    filepath = rootstatsdir + statsdirlocation + datetimefile + "/";

    updateDeviceList();
    serialPortPowerMeter= new SerialPortInterface();
    Q_EMIT(ondevice_comboBox_currentIndexChanged());
    connect(ui->device_comboBox, &QComboBox::currentIndexChanged, this, &MainWindow::ondevice_comboBox_currentIndexChanged);
    connect(serialPortPowerMeter,&SerialPortInterface::serialPortNewRFData,this,&MainWindow::updateData);
    connect(serialPortPowerMeter,&SerialPortInterface::serialPortErrorSignal,this,&MainWindow::on_serialPortError);
    connect(serialPortPowerMeter, &SerialPortInterface::portOpened, this, &MainWindow::onPortOpened);
    connect(serialPortPowerMeter, &SerialPortInterface::portClosed, this, &MainWindow::onPortClosed);
    connect(this, &MainWindow::isConnectedChanged, this, &MainWindow::onIsConnectedChanged);


    ui->resetMax_toolButton->setToolTip(tr("Reset max values"));
    ui->resetMax_toolButton->setIcon(QIcon::fromTheme("process-stop",QIcon(":/images/process-stop.svg")));

    ui->browse_toolButton->setToolTip(tr("Browse saved data directory"));
    ui->browse_toolButton->setIcon(QIcon::fromTheme("document-open",QIcon(":/images/document-open.svg")));

    connect(ui->browse_toolButton, &QToolButton::clicked, this, [=]() {
        if (QDir(filepath).exists()) QDesktopServices::openUrl(QUrl::fromLocalFile(filepath));
        else if(QDir(rootstatsdir+statsdirlocation).exists()) QDesktopServices::openUrl(QUrl::fromLocalFile(rootstatsdir+statsdirlocation));
        else QToolTip::showText(QCursor::pos(), tr("Session data not saved yet"), ui->browse_toolButton);
    });

    data_model=new QStandardItemModel(0,0,this);
    data_model->setHorizontalHeaderItem(dataTimeColumnID,new QStandardItem(QString(tr("Time"))));
    data_model->setHorizontalHeaderItem(dataValuedBmColumnID,new QStandardItem(QString(tr("dBm"))));
    data_model->setHorizontalHeaderItem(dataValuemVppColumnID,new QStandardItem(QString(tr("mVpp"))));
    data_model->setHorizontalHeaderItem(dataValuemWColumnID,new QStandardItem(QString(tr("mW"))));
    data_model->setHorizontalHeaderItem(dataValueFreqColumnID,new QStandardItem(QString(tr("Frequency MHz"))));
    data_model->setHorizontalHeaderItem(dataValueCorrectColumnID,new QStandardItem(QString(tr("Correction dB"))));
    data_model->setHorizontalHeaderItem(dataValueAttenuationColumnID,new QStandardItem(QString(tr("Attenuation dB"))));
    data_model->setHorizontalHeaderItem(dataValueTotalDbmColumnID,new QStandardItem(QString(tr("Total dBm"))));
    data_model->setHorizontalHeaderItem(dataValueTotalMwColumnID,new QStandardItem(QString(tr("Total mW"))));


    ui->data_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->data_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->data_tableView->setModel(data_model);
    ui->data_tableView->setColumnWidth(dataTimeColumnID,200);
    ui->data_tableView->setColumnWidth(dataValueFreqColumnID,200);
    ui->data_tableView->setColumnWidth(dataValueCorrectColumnID,200);
    ui->data_tableView->setColumnWidth(dataValueAttenuationColumnID,200);
    ui->data_tableView->setColumnWidth(dataValueTotalDbmColumnID,200);
    ui->data_tableView->setColumnWidth(dataValueTotalMwColumnID,200);

    connect(ui->data_tableView->model(),SIGNAL(rowsInserted(QModelIndex,int,int)),SLOT(on_data_model_rowsInserted(QModelIndex,int,int)));

    ui->dbm_lcdNumber->setSegmentStyle(QLCDNumber::Flat );
    ui->dbm_lcdNumber->setStyleSheet("QLCDNumber{ background-color: green; color: yellow;}");
    ui->mW_lcdNumber->setSegmentStyle(QLCDNumber::Flat );
    ui->mW_lcdNumber->setDigitCount(6);
    ui->mW_lcdNumber->setStyleSheet("QLCDNumber{ background-color: green; color: yellow;}");
    ui->mVpp_lcdNumber->setSegmentStyle(QLCDNumber::Flat );
    ui->mVpp_lcdNumber->setStyleSheet("QLCDNumber{ background-color: green; color: yellow;}");

    connect(&simulatorTimer, SIGNAL(timeout()), this, SLOT(on_simulatorTimer()));

    //read rules for charts from file
    QString configpath = "";
#ifdef Q_OS_WIN
    configpath = qApp->applicationDirPath();
#endif
#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
    configpath = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation)[0] + "/" + qAppName();
#endif
 /*   QFile file(configpath + "/chart_rules.json");
    QDir().mkpath(configpath) ;
    if (!QFileInfo::exists(configpath + "/chart_rules.json"))
    {
        QFile::copy(":/chart_rules.json", configpath + "/chart_rules.json");
        QFile(configpath + "/chart_rules.json").setPermissions(QFile::WriteUser | QFile::WriteOwner | QFile::WriteGroup | QFile::ReadUser | QFile::ReadOwner | QFile::ReadGroup);
    }

    file.open(QIODevice::ReadOnly);
    QString jsonRules = file.readAll();
    file.close();
*/
    const QString overrideFilePath = configpath + "/force_chart_rules.json";
    const QString resourceFilePath = ":/chart_rules.json";
    QString jsonRules;
    QFile file;

    if (QFileInfo::exists(overrideFilePath))
    {
        qDebug()<<"Loading user override chart rules from:"<<overrideFilePath;
        file.setFileName(overrideFilePath);
    }
    else
    {
        qDebug()<<"Loading default chart rules from resources:"<<resourceFilePath;
        file.setFileName(resourceFilePath);
    }

    if (file.open(QIODevice::ReadOnly))
    {
        jsonRules = file.readAll();
        file.close();
    }
    else
    {
        qWarning()<<"Could not open chart rules file:"<<file.fileName()<<file.errorString();
    }


    QString jsonChartsRuleStr;
    QJsonParseError e;
    QJsonDocument jsonDocRules = QJsonDocument::fromJson(jsonRules.toUtf8(), &e);
    if (e.error == QJsonParseError::NoError)
    {
        if (!jsonDocRules.isNull() && !jsonDocRules.isEmpty())
        {
            jsonChartsRuleStr = QJsonDocument::fromVariant(jsonDocRules.object()["chartrules"].toObject().toVariantMap()).toJson(QJsonDocument::Compact);
            qDebug()<<jsonChartsRuleStr;
        }

    }
    else
    {
        qWarning()<<"Error incorrect json chartrules!!! "<<e.errorString()<<" "<<e.offset;
    }



    charts = new chartManager(ui->charts_scrollArea->widget());
    charts->setstrDateTimeFile(datetimefile);
    charts->setjsonChartRuleObject(jsonChartsRuleStr);
    charts->setisflow(ui->flow_checkBox->isChecked());
    charts->connectTracers();
    connect(this, &MainWindow::newData, charts, &chartManager::dataIncome);

    attenuationMgr = new AttenuationManager(this);
    ui->att_gridLayout->addWidget(attenuationMgr, 1, 0, 1, 1);
    connect(attenuationMgr, &AttenuationManager::totalAttenuationChanged, this, &MainWindow::onTotalAttenuationChanged);
    ui->att_pushButton->setText(tr("Attenuation:") + " " + QString::number(m_current_atteuation,'f',2) + " dB");

    m_attenuatorCalculator = new TargetPowerCalculator(this);
    // Configure the calculator externally
    m_attenuatorCalculator->setMinDbm(-45.0);
    m_attenuatorCalculator->setMaxDbm(-5.0);
    m_attenuatorCalculator->setTargetDbm(-20.0);
    ui->att_gridLayout->addWidget(m_attenuatorCalculator, 2, 0, 1, 1);


    // Connect the AttenuationManager to the calculator
    connect(attenuationMgr, &AttenuationManager::totalAttenuationChanged, m_attenuatorCalculator, &TargetPowerCalculator::onActualAttenuationChanged);

    ui->gridLayout_6->setColumnStretch(0, 1);
    ui->gridLayout_6->setColumnStretch(1, 0);


    connect(ui->att_pushButton, &QPushButton::toggled, ui->attenuation_dockWidget, &QWidget::setVisible);
    ui->attenuation_dockWidget->installEventFilter(this);

    // Set initial state
    ui->att_pushButton->setChecked(false);
    ui->attenuation_dockWidget->setVisible(false);

    // --- Calibration Setup ---
    m_calibrationManager = new CalibrationManager(this);
    ui->calibration_dockWidget->setWidget(m_calibrationManager);
    ui->calibration_dockWidget->setVisible(false);
    connect(ui->calibration_pushButton, &QPushButton::toggled, this, &MainWindow::on_calibration_pushButton_toggled);
    connect(m_calibrationManager, &CalibrationManager::frequencySelected, this, &MainWindow::onCalibrationFrequencySelected);
    ui->calibration_dockWidget->installEventFilter(this);
    connect(this, &MainWindow::newMeasurement, m_calibrationManager, &CalibrationManager::onNewMeasurement);

    QMainWindow::tabifyDockWidget(ui->attenuation_dockWidget, ui->calibration_dockWidget);


    Q_EMIT(on_set_pushButton_clicked());


    ui->range_spinBox->setMinimum(1);
    ui->range_spinBox->setMaximum(100000);
    ui->range_spinBox->setAlignment(Qt::AlignRight);
    ui->range_spinBox->setValue(10);

    ui->flow_checkBox->setText(tr("Flow"));
    ui->flow_checkBox->setToolTip(tr("After this time data on chart will move out so it will look like a flow"));

    ui->refreshDevices_toolbutton->setToolTip(tr("Refresh Devices"));
    ui->refreshDevices_toolbutton->setText(tr("Refresh"));
    ui->refreshDevices_toolbutton->setIcon(QIcon::fromTheme("view-refresh",QIcon(":/images/view-refresh.svg")));

    ui->resetCharts_toolButton->setIcon(QIcon::fromTheme("process-stop",QIcon(":/images/process-stop.svg")));

    ui->saveCharts_toolButton->setIcon(QIcon::fromTheme("document-save",QIcon(":/images/document-save.svg")));
    ui->saveCharts_toolButton->setText(tr("Save charts"));
    ui->saveCharts_toolButton->setToolTip(tr("Save charts as images to the log folder"));

    ui->imageFormat_comboBox->setToolTip(tr("Choose output format"));
    ui->imageFormat_comboBox->addItem("png");
    ui->imageFormat_comboBox->addItem("jpg");

    ui->imageWidth_spinBox->setToolTip(tr("Set image width"));
    ui->imageWidth_spinBox->setMinimum(0);
    ui->imageWidth_spinBox->setMaximum(5000);
    ui->imageWidth_spinBox->setAlignment(Qt::AlignRight);
    ui->imageWidth_spinBox->setValue(1366);

    ui->imageHeight_spinBox->setToolTip(tr("Set image height"));
    ui->imageHeight_spinBox->setMinimum(0);
    ui->imageHeight_spinBox->setMaximum(5000);
    ui->imageHeight_spinBox->setAlignment(Qt::AlignRight);
    ui->imageHeight_spinBox->setValue(768);

    ui->writeCSV_checkBox->setText(tr("csv"));
    ui->writeCSV_checkBox->setToolTip(tr("Write on fly data to csv file into log folder"));

    connect(ui->resetCharts_toolButton,&QToolButton::clicked,charts,&chartManager::resetAllCharts);
    connect(ui->flow_checkBox,&QCheckBox::stateChanged,charts,&chartManager::setisflow);
    ui->flow_checkBox->setChecked(true);

    //autoconnector works fine
    //connect(ui->range_spinBox, SIGNAL(valueChanged(int)), this, SLOT(on_range_spinBox_valueChanged(int)));
    //connect(ui->saveCharts_toolButton, SIGNAL(clicked()), this, SLOT(on_saveCharts_toolButton_clicked()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

/* slot to call range changing */
void MainWindow::on_range_spinBox_valueChanged(int range)
{
    qDebug()<<"on_range_spinBox_valueChanged";
    //convert to minutes
    charts->setAllRanges(range * 60);
}

int MainWindow::on_saveCharts_toolButton_clicked()
{
    qDebug()<<"on_saveCharts_toolButton_clicked";
    QString chartdatetime = QDateTime::fromMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch()).toString("dd.MM.yyyy_hh.mm.ss.zzz");
    QString chartfoldername = "saved_charts";
    QString chart_filepath = filepath + "/" + chartfoldername + "/" + chartdatetime + "/";
    if (!createDir(chart_filepath))
    {
        qDebug()<<"no such directory"<<chart_filepath;
    }
    else
    {
        charts->saveAllCharts(chart_filepath + "/", ui->imageFormat_comboBox->currentText(), ui->imageWidth_spinBox->value(), ui->imageHeight_spinBox->value());

    }
    //show messagebox that saved (really I'm not checking if it's saved or not :) )
    QMessageBox msgbox;

    msgbox.setIcon(QMessageBox::Information);
    msgbox.setWindowTitle(tr("Info"));
    msgbox.setText(tr("Saved"));
    QRect widgetRect = this->geometry();
    msgbox.move(this->parentWidget()->mapToGlobal(widgetRect.center()));
    msgbox.exec();

    return 0;

}


void MainWindow::ondevice_comboBox_currentIndexChanged()
{
    qDebug()<<"ondevice_comboBox_currentIndexChanged";
    onIsConnectedChanged(isConnected());
}

void MainWindow::updateData(const QString &data)
{
    qDebug()<<"updateData"<<data;
    QString curdate=QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz");
    ui->data_plainTextEdit->appendPlainText(curdate+" "+ui->device_comboBox->currentData().toString()+" "+data);

    QRegularExpression reg("[$]([-+][0-9.]+)dBm([0-9.]+)(.)Vpp[$]");

    QRegularExpressionMatchIterator i = reg.globalMatch(data.simplified().replace(" ",""));
    if (i.isValid())
    {
        while (i.hasNext())
        {
            QRegularExpressionMatch match = i.next();

            bool ok;
            double dbm = match.captured(1).toDouble(&ok);
            double milliwatts = 0;

            if (ok)
            {
                // --- Apply Corrections for Display ---
                double current_freq_mhz = getFrequency().toDouble();
                double calibration_correction = m_calibrationManager->getCorrection(current_freq_mhz);

                double actual_dbm = dbm + m_current_atteuation + calibration_correction;
                double actual_milliwatts = UnitConverter::dBmToMilliwatts(actual_dbm);

                // Update main displays with actual values
                ui->dbm_lcdNumber->display(actual_dbm);
                emit newMeasurement(dbm);
                QPair<double, QString> formattedPower = UnitConverter::formatPower(actual_milliwatts);
                ui->mW_lcdNumber->display(formattedPower.first);
                ui->wattage_groupBox->setTitle(formattedPower.second);

                // Use UnitConverter to get Vpp from actual power
                double vpp = UnitConverter::milliwattsToVpp(actual_milliwatts);
                QPair<double, QString> formattedVoltage = UnitConverter::formatVoltage(vpp);
                ui->mVpp_lcdNumber->display(formattedVoltage.first);
                // Correctly append "Vpp" to the prefix (k, "", m, µ, n)
                ui->mvpp_groupBox->setTitle(formattedVoltage.second + "Vpp");

                // Update MAX displays with actual values
                if(ui->maxdbm_lineEdit->text().toDouble() < actual_dbm || ui->maxdbm_lineEdit->text().isEmpty())
                {
                    ui->maxdbm_lineEdit->setText(QString::number(actual_dbm, 'f', 2));
                    QPair<double, QString> formattedMaxPower = UnitConverter::formatPower(actual_milliwatts);
                    ui->maxmw_lineEdit->setText(QString::number(formattedMaxPower.first, 'f', 4));
                    ui->maxmw_label->setText(formattedMaxPower.second + ":");
                }
            }
            else
            {
                // Display error on LCDs if parsing failed
                ui->dbm_lcdNumber->display(tr("Error"));
                ui->mW_lcdNumber->display(tr("Error"));
                ui->mVpp_lcdNumber->display(tr("Error"));
                ui->wattage_groupBox->setTitle("mW");
                ui->mvpp_groupBox->setTitle("mVpp");
            }

            // --- Table Population Logic ---
            int row = data_model->rowCount();
            data_model->setItem(row,dataTimeColumnID,new QStandardItem(curdate));
            data_model->setItem(row,dataValuedBmColumnID,new QStandardItem(match.captured(1)));
            data_model->setData(data_model->index(row,dataValuedBmColumnID),match.captured(1),Qt::UserRole);


            if(ok)
            {
                milliwatts = UnitConverter::dBmToMilliwatts(dbm);
                data_model->setItem(row,dataValuemWColumnID,new QStandardItem(QString::number(milliwatts,'f',7)));
                data_model->setData(data_model->index(row,dataValuemWColumnID),QString::number(milliwatts,'f',7),Qt::UserRole);
            }
            else
            {
                data_model->setItem(row,dataValuemWColumnID,new QStandardItem(tr("Error")));
            }

            double mvpp_raw=match.captured(2).toDouble();
            if(match.captured(3)=='u') mvpp_raw /= 1000.0;
            data_model->setItem(row,dataValuemVppColumnID,new QStandardItem(QString::number(mvpp_raw,'f',4)));
            data_model->setData(data_model->index(row,dataValuemVppColumnID),mvpp_raw,Qt::UserRole);

            data_model->setItem(row,dataValueFreqColumnID,new QStandardItem(getFrequency()));
            data_model->setData(data_model->index(row,dataValueFreqColumnID),getFrequency(),Qt::UserRole);
            data_model->setItem(row,dataValueCorrectColumnID,new QStandardItem(getOffset()));
            data_model->setData(data_model->index(row,dataValueCorrectColumnID),getOffset(),Qt::UserRole);

            data_model->setItem(row, dataValueAttenuationColumnID, new QStandardItem(QString::number(m_current_atteuation, 'f', 2)));

            if (ok) {
                double calibration_correction_for_table = m_calibrationManager->getCorrection(getFrequency().toDouble());
                double total_dbm = dbm + m_current_atteuation + calibration_correction_for_table;
                double total_mw = UnitConverter::dBmToMilliwatts(total_dbm);

                data_model->setItem(row, dataValueTotalDbmColumnID, new QStandardItem(QString::number(total_dbm, 'f', 2)));
                data_model->setItem(row, dataValueTotalMwColumnID, new QStandardItem(QString::number(total_mw, 'f', 7)));
            } else {
                data_model->setItem(row, dataValueTotalDbmColumnID, new QStandardItem(tr("Error")));
                data_model->setItem(row, dataValueTotalMwColumnID, new QStandardItem(tr("Error")));
            }

                QString logLine="";
                QString headersList="";
                for (int i = 0; i < data_model->columnCount(); i++)
                {
                    headersList = headersList + data_model->headerData(i, Qt::Horizontal).toString() + ",";
                    if (data_model->hasIndex(row, i))
                    {
                        logLine = logLine + data_model->data(data_model->index(row, i)).toString() + ",";
                    }
                    else
                    {
                        qDebug()<<"no such index";
                    }
                }
                if (headersList != "")
                {
                    headersList = headersList.left(headersList.length() - 1);
                }
                if (logLine != "")
                {
                    logLine = logLine.left(logLine.length() - 1);
                }
                qDebug()<<headersList;
                qDebug()<<logLine;


            if(ui->writeCSV_checkBox->isChecked())
            {
                writeStatCSV("power", logLine, headersList);
            }
            emit newData(headersList, logLine);
        }
    }
    else
    {
        ui->statusbar->showMessage(tr("Error incoming data, check device"),1000);
    }
}

void MainWindow::on_connect_pushButton_clicked()
{
    qDebug()<<"on_connect_pushButton_clicked";
    if (ui->device_comboBox->currentIndex() == -1)
    {
        qDebug()<<"Connect clicked with no device selected.";
        return;
    }
    // This is the commit point. Set the current device from the UI selection.
    QString selectedPort = ui->device_comboBox->currentData(PortNameRole).toString();
    setCurrentDevice(selectedPort);
    qDebug()<<"Attempting to connect to"<<currentDevice();

    if(!currentDevice().isEmpty())
    {
        serialPortPowerMeter->setportName(currentDevice());
        serialPortPowerMeter->setbaudRate(9600);
        serialPortPowerMeter->startPort();
    }

}

void MainWindow::on_disconnect_pushButton_clicked()
{
    qDebug()<<"on_disconnect_pushButton_clicked";
    serialPortPowerMeter->stopPort();

}

void MainWindow::on_refreshDevices_toolbutton_clicked()
{
    qDebug()<<"on_refreshDevices_toolbutton_clicked";
    updateDeviceList();
}

void MainWindow::updateDeviceList()
{
    qDebug()<<"updateDeviceList";
    //QString previouslySelectedPort = "";
    // If a device is already selected, remember its port name.
    //if (ui->device_comboBox->currentIndex() != -1)
    //{
    //    previouslySelectedPort = ui->device_comboBox->currentData(PortNameRole).toString();
    //}
    //qDebug()<<"Previously selected port:"<<previouslySelectedPort;

    ui->device_comboBox->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    int newIndexToSelect = -1;
    for (const QSerialPortInfo &info : infos)
    {
        if (info.isNull() || !info.hasVendorIdentifier() || QString::number(info.vendorIdentifier(),16) != "1a86")
            continue;

        qDebug()<<info.hasVendorIdentifier() <<QString::number(info.vendorIdentifier());
        bool isBusy = false;
        QSerialPort tempPort(info);
        if (!tempPort.open(QIODevice::ReadWrite))
        {
            isBusy = true;
        }
        else
        {
            tempPort.close();
        }
        QString busyText = isBusy ? tr(" [Busy]") : "";
        QString s = tr("Port") + ": " + info.portName() +
                    " (" + info.systemLocation() +
                    ") " + info.description() +
                    " " + info.manufacturer() +
                    " " + info.serialNumber() +
                    " (" + (info.hasVendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : QString()) +
                    ":" + (info.hasProductIdentifier() ? QString::number(info.productIdentifier(), 16) : QString()) +")"+
                    busyText;
        qInfo()<<info.portName();

        qInfo()<<s;
        int newIndex = ui->device_comboBox->count();
        ui->device_comboBox->addItem(s);
        ui->device_comboBox->setItemData(newIndex, info.portName(), PortNameRole);
        ui->device_comboBox->setItemData(newIndex, info.systemLocation(), SystemLocationRole);
        ui->device_comboBox->setItemData(newIndex, info.description(), DescriptionRole);
        ui->device_comboBox->setItemData(newIndex, info.manufacturer(), ManufacturerRole);
        ui->device_comboBox->setItemData(newIndex, info.serialNumber(), SerialNumberRole);
        ui->device_comboBox->setItemData(newIndex, (info.hasVendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : QString()), VendorIDRole);
        ui->device_comboBox->setItemData(newIndex, (info.hasProductIdentifier() ? QString::number(info.productIdentifier(), 16) : QString()), ProductIDRole);
        ui->device_comboBox->setItemData(newIndex, isBusy, IsBusyRole);

        if (info.portName() == currentDevice())
        {
            newIndexToSelect = newIndex;
            if (newIndexToSelect != -1)
            {
                ui->device_comboBox->setCurrentIndex(newIndexToSelect);
            }
        }
    }

    if (newIndexToSelect != -1)
    {
        ui->device_comboBox->setCurrentIndex(newIndexToSelect);
    }
    else
    {
        if(isConnected())onPortClosed();
    }
    ondevice_comboBox_currentIndexChanged();
}

void MainWindow::onPortOpened()
{
    qDebug()<<Q_FUNC_INFO;
    setDeviceError("");
    setIsConnected(true);
    Q_EMIT updateDeviceList();
    Q_EMIT on_set_pushButton_clicked();
}

void MainWindow::onPortClosed()
{
    qDebug()<<Q_FUNC_INFO;
    setIsConnected(false);
    Q_EMIT updateDeviceList();
}

void MainWindow::on_serialPortError(const QString &error)
{
    qDebug()<<"on_serialPortError";
    setDeviceError(error);
    setIsConnected(false);
    Q_EMIT updateDeviceList();
}

void MainWindow::onIsConnectedChanged(bool connected)
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
        ui->statusbar->showMessage(tr("Connected to %1").arg(currentDevice()));
    }
    else
    {
        if (!deviceError().isEmpty())
        {
            ui->statusbar->showMessage(tr("Error: %1").arg(deviceError()));
        }
        else
        {
            ui->statusbar->showMessage(tr("Disconnected"));
        }
    }
}


void MainWindow::on_data_model_rowsInserted(const QModelIndex & parent, int start, int end)
{
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    qDebug()<<"on_data_model_rowsInserted"<<start;
    if(ui->data_tableView->verticalScrollBar()->value()==ui->data_tableView->verticalScrollBar()->maximum())
    {
        ui->data_tableView->scrollToBottom();
    }

}

void MainWindow::on_simulate_checkBox_clicked()
{
    qDebug()<<"on_simulate_checkBox_clicked";
    if(ui->simulate_checkBox->isChecked())
    {

        if (!simulatorTimer.isActive())
        {
            simulatorTimer.start(500);
        }
    }
    else
    {
        if (simulatorTimer.isActive())
        {
            simulatorTimer.stop();
        }
    }
}

void MainWindow::on_simulatorTimer()
{

    double simdbmvalue =  QRandomGenerator::global()->generateDouble() * (5 + 60) - 60;
    qDebug()<<"on_simulatorTimer simulator is in progress";
    QString formattedValue = (simdbmvalue >= 0) ? QString("+%1").arg(simdbmvalue, 0, 'f', 1) : QString::number(simdbmvalue, 'f', 1);
    //2×(((2×impedance)÷1000)^(1÷2))×(10^(simdbmvalue÷20))
    //2×(((2×50)÷1000)^(1÷2))=0.6324555322
    double simmvppValue = qPow(10, (simdbmvalue / 20.0)) * 0.6324555322*1000000;
    char simmvppSign='u';
    if(simmvppValue>1000)
    {
        simmvppValue=simmvppValue/1000;
        simmvppSign='m';
    }
    QString formattedvppValue =  QString::number(simmvppValue, 'f', 1);
    QString formattedString = QString("$%1 dBm%2 %3Vpp$").arg(formattedValue).arg(formattedvppValue).arg(simmvppSign);

    Q_EMIT(updateData(formattedString));

}


void MainWindow::on_set_pushButton_clicked()
{
    qDebug()<<"on_set_pushButton_clicked";
    setFrequency(QString::number(ui->frequency_spinBox->value(),'d',0).rightJustified(4, '0'));
    setOffset(QString(ui->correctionplus_radioButton->isChecked()?"+":"-")+QString::number(ui->offset_doubleSpinBox->value(),'f',1).rightJustified(4, '0'));
    qDebug()<<getFrequency();
    qDebug()<<getOffset();
    serialPortPowerMeter->writeData(("$"+getFrequency()+getOffset()+"#").toLatin1());
}

void MainWindow::writeStatCSV(const QString &appendFileName, const QString &logLine, const QString &headersList)
{
    QTextStream cout(stdout);
    if (!createDir(filepath))
    {
        qDebug()<<"no such directory"<<filepath;
    }
    QString statfilepath = filepath + appendFileName + ".csv";
    bool exists = false;
    if (!QFile(statfilepath).exists())
    {
        exists = false;
    }
    else
    {
        exists = true;
    }
    QFile outFile(statfilepath);
    //read current amount of columns
    //maybe it will be better to use some in memory map  for performance
    //so if some problems will be here i will use QMap as cache for column amount
    if (exists)
    {
        if (!outFile.open(QIODevice::ReadOnly | QIODevice::Text))return;
        int filecolumns = QString(outFile.readLine()).split(",").count();
        outFile.close();
        int inputcolumns = headersList.split(",").count();
        if (inputcolumns > filecolumns)
        {
            //int addcommaamount=inputcolumns-filecolumns;
            //call rewrite file
            QFile newFile(statfilepath + ".new");
            if (!(outFile.open(QIODevice::ReadOnly | QIODevice::Text) && newFile.open(QIODevice::WriteOnly | QIODevice::Append)))return;
            {
                QTextStream textdata(&outFile);
                QTextStream textStream(&newFile);
                textStream<<headersList<<Qt::endl;
                int linecnt = 0;
                while (!textdata.atEnd())
                {
                    if (linecnt > 0)
                    {
                        textStream<<textdata.readLine();
                    }
                    else
                    {
                        textdata.readLine();
                    }
                    linecnt++;
                }
                //replace original with new one
                outFile.remove();
                newFile.rename(statfilepath);

            }

        }
    }
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Append))return;
    QTextStream textStream(&outFile);
    if (!exists)
    {
        textStream<<headersList<<Qt::endl<<logLine<<Qt::endl;
    }
    else
    {
        textStream<<logLine<<Qt::endl;
    }
    outFile.close();
}

bool MainWindow::createDir(QString path)
{
    if (!QDir(path).exists())
    {
        qDebug() <<"Creating stats dir: "<<path;
        if (QDir().mkpath(path))
        {
            qDebug() <<"Creating succesfull!";
        }
        else
        {
            qDebug() <<"unable to create: "<<path;
        }
    }
    else
    {
        qDebug() <<"Directory alredy exists: "<<path;
    }
    if (!QFile(path).exists())
    {
        qDebug()<<"imposible to create folder";
        return false;
    }
    else
    {
        return true;
    }
}


void MainWindow::onTotalAttenuationChanged(double totalAttenuation)
{
    qDebug()<<"Total attenuation changed:"<<totalAttenuation;
    ui->att_pushButton->setText(tr("Attenuation:") + " " + QString::number(totalAttenuation,'f',2) + " dB");
    m_current_atteuation=totalAttenuation;

}

void MainWindow::on_resetMax_toolButton_clicked()
{
    qDebug()<<Q_FUNC_INFO;
    ui->maxdbm_lineEdit->setText("");
    ui->maxmw_lineEdit->setText("");
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->attenuation_dockWidget && event->type() == QEvent::Close)
    {
        ui->att_pushButton->setChecked(false);
        return false;
    }
    if (watched == ui->calibration_dockWidget && event->type() == QEvent::Close)
    {
        ui->calibration_pushButton->setChecked(false);
        return false;
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::on_calibration_pushButton_toggled(bool checked)
{
    ui->calibration_dockWidget->setVisible(checked);
}

void MainWindow::onCalibrationFrequencySelected(double frequencyMHz)
{
    qDebug()<<"Calibration: A frequency was selected:"<<frequencyMHz<<"MHz. Please set your signal generator accordingly.";
    ui->frequency_spinBox->setValue(static_cast<int>(frequencyMHz));
    on_set_pushButton_clicked();
}
