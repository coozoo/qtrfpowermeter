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
    connect(ui->device_comboBox, &QComboBox::currentTextChanged, this, &MainWindow::ondevice_comboBox_currentIndexChanged);
    connect(serialPortPowerMeter,&SerialPortInterface::serialPortNewData,this,&MainWindow::updateData);
    connect(serialPortPowerMeter,&SerialPortInterface::serialPortErrorSignal,this,&MainWindow::on_serialPortError);




    data_model=new QStandardItemModel(0,0,this);
    data_model->setHorizontalHeaderItem(dataTimeColumnID,new QStandardItem(QString(tr("Time"))));
    data_model->setHorizontalHeaderItem(dataValuedBmColumnID,new QStandardItem(QString(tr("dBm"))));
    data_model->setHorizontalHeaderItem(dataValuemVppColumnID,new QStandardItem(QString(tr("mVpp"))));
    data_model->setHorizontalHeaderItem(dataValuemWColumnID,new QStandardItem(QString(tr("mW"))));
    data_model->setHorizontalHeaderItem(dataValueFreqColumnID,new QStandardItem(QString(tr("Frequency MHz"))));
    data_model->setHorizontalHeaderItem(dataValueCorrectColumnID,new QStandardItem(QString(tr("Correction dB"))));


    ui->data_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->data_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->data_tableView->setModel(data_model);
    ui->data_tableView->setColumnWidth(dataTimeColumnID,200);
    ui->data_tableView->setColumnWidth(dataValueFreqColumnID,200);
    ui->data_tableView->setColumnWidth(dataValueCorrectColumnID,200);


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
    QFile file(configpath + "/chart_rules.json");
    QDir().mkpath(configpath) ;
    if (!QFileInfo::exists(configpath + "/chart_rules.json"))
    {
        QFile::copy(":/chart_rules.json", configpath + "/chart_rules.json");
        QFile(configpath + "/chart_rules.json").setPermissions(QFile::WriteUser | QFile::WriteOwner | QFile::WriteGroup | QFile::ReadUser | QFile::ReadOwner | QFile::ReadGroup);
    }

    file.open(QIODevice::ReadOnly);
    QString jsonRules = file.readAll();
    file.close();
    QString jsonChartsRuleStr;
    QJsonParseError e;
    QJsonDocument jsonDocRules = QJsonDocument::fromJson(jsonRules.toUtf8(), &e);
    if (e.error == QJsonParseError::NoError)
    {
        if (!jsonDocRules.isNull() && !jsonDocRules.isEmpty())
        {
            jsonChartsRuleStr = QJsonDocument::fromVariant(jsonDocRules.object()["chartrules"].toObject().toVariantMap()).toJson(QJsonDocument::Compact);
            qDebug() << jsonChartsRuleStr;
        }

    }
    else
    {
        qWarning() << "Error incorrect json chartrules!!! " << e.errorString() << " " << e.offset;
    }



    charts = new chartManager(ui->charts_scrollArea->widget());
    charts->setstrDateTimeFile(datetimefile);
    charts->setjsonChartRuleObject(jsonChartsRuleStr);
    charts->setisflow(ui->flow_checkBox->isChecked());
    connect(this, SIGNAL(newData(QString, QString)), charts, SLOT(dataIncome(QString, QString)));
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
    //autoconnector works fine
    //connect(ui->range_spinBox, SIGNAL(valueChanged(int)), this, SLOT(on_range_spinBox_valueChanged(int)));
    //connect(ui->saveCharts_toolButton, SIGNAL(clicked()), this, SLOT(on_saveCharts_toolButton_clicked()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

/* slot to coll range changing */
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
        qDebug() << "no such directory"<<chart_filepath;
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

void MainWindow::updateData(QString data)
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
            qDebug()<<match.captured(1);
            ui->dbm_lcdNumber->display(match.captured(1));
            bool ok;
            double mw=0;
            double dbm=match.captured(1).toDouble(&ok);
            mw=dBmTomW(dbm);
            if(ok)
            {
                ui->mW_lcdNumber->display(mw);
                if(ui->maxdbm_lineEdit->text().toDouble()<dbm || ui->maxdbm_lineEdit->text().isEmpty())
                {
                    ui->maxmw_lineEdit->setText(QString::number(mw,'f',4));
                    ui->maxdbm_lineEdit->setText(match.captured(1));
                }
            }
            else
            {
                ui->mW_lcdNumber->display(tr("Error"));
            }

            double mvpp=0;
            QString mvppformatted="0";
            qDebug()<<match.captured(2)<<match.captured(3);
            if(match.captured(3)=='u')
            {
                mvpp=match.captured(2).toDouble()/1000;
                mvppformatted=QString::number(mvpp,'f',4);
            }
            else
            {
                mvpp=match.captured(2).toDouble();
                mvppformatted=QString::number(mvpp,'f',1);
            }
            ui->mVpp_lcdNumber->display(mvppformatted);
            int row=data_model->rowCount();
            data_model->setItem(row,dataTimeColumnID,new QStandardItem(curdate));
            data_model->setItem(row,dataValuedBmColumnID,new QStandardItem(match.captured(1)));
            data_model->setData(data_model->index(row,dataValuedBmColumnID),match.captured(1),Qt::UserRole);
            emit newData(data_model->headerData(dataValuedBmColumnID, Qt::Horizontal ).toString(), match.captured(1));
            if(ok)
            {
                data_model->setItem(row,dataValuemWColumnID,new QStandardItem(QString::number(mw,'g',5)));
                data_model->setData(data_model->index(row,dataValuemWColumnID),QString::number(mw,'g',5),Qt::UserRole);
            }
            data_model->setItem(row,dataValuemVppColumnID,new QStandardItem(mvppformatted));
            data_model->setData(data_model->index(row,dataValuemVppColumnID),mvppformatted,Qt::UserRole);
            data_model->setItem(row,dataValueFreqColumnID,new QStandardItem(getFrequency()));
            data_model->setData(data_model->index(row,dataValueFreqColumnID),getFrequency(),Qt::UserRole);
            data_model->setItem(row,dataValueCorrectColumnID,new QStandardItem(getOffset()));
            data_model->setData(data_model->index(row,dataValueCorrectColumnID),getOffset(),Qt::UserRole);

            if(ui->writeCSV_checkBox->isChecked())
            {
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
                writeStatCSV("power", logLine, headersList);
            }
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

        if(!ui->device_comboBox->currentData().toString().isEmpty())
        {
        serialPortPowerMeter->setportName(ui->device_comboBox->currentData().toString());
        serialPortPowerMeter->setbaudRate(9600);
        serialPortPowerMeter->startPort();
        Q_EMIT(on_set_pushButton_clicked());
        updateDeviceList();
       }

}

void MainWindow::on_disconnect_pushButton_clicked()
{
       qDebug()<<"on_disconnect_pushButton_clicked";
       serialPortPowerMeter->stopPort();
       updateDeviceList();

}

void MainWindow::on_refreshDevices_toolbutton_clicked()
{
    qDebug()<<"on_refreshDevices_toolbutton_clicked";
    updateDeviceList();
}

void MainWindow::updateDeviceList()
{
    qDebug()<<"updateDeviceList";
    ui->device_comboBox->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos)
    {
        if (info.isNull())
            continue;

        qDebug()<<info.hasVendorIdentifier() <<QString::number(info.vendorIdentifier());
        if(info.hasVendorIdentifier() && QString::number(info.vendorIdentifier(),16)=="1a86")
        {
            QString busyText;
            QSerialPort serialPort(info);
            if (!serialPort.open(QIODevice::ReadWrite)) {
                busyText = tr(" [Busy]");
            } else {
                serialPort.close(); // Close immediately if opened
                busyText = "";
            }
            QString s = tr("Port: ") + info.portName() +
                        tr(" (") + info.systemLocation() +
                        tr(") ") + info.description() +
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

double MainWindow::dBmTomW(double dbm)
{
    double conv=qPow(10,(dbm/10));
    return conv;
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
    //if(ui->correctionplus_radioButton->isChecked())
    //{
    setOffset(QString(ui->correctionplus_radioButton->isChecked()?"+":"-")+QString::number(ui->offset_doubleSpinBox->value(),'f',1).rightJustified(4, '0'));
    //}
    qDebug()<<getFrequency();
    qDebug()<<getOffset();
    serialPortPowerMeter->writeData(("$"+getFrequency()+getOffset()+"#").toLatin1());
}

void MainWindow::on_serialPortError(QString error)
{
    qDebug()<<"on_serialPortError";
    ui->statusbar->showMessage(error,1000);
    Q_EMIT(ondevice_comboBox_currentIndexChanged());
}

void MainWindow::writeStatCSV(QString appendFileName, QString logLine, QString headersList)
{
    QTextStream cout(stdout);
    if (!createDir(filepath))
    {
        qDebug() << "no such directory"<<filepath;
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
                textStream << headersList << Qt::endl;
                int linecnt = 0;
                while (!textdata.atEnd())
                {
                    if (linecnt > 0)
                    {
                        textStream << textdata.readLine();
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
        textStream << headersList << Qt::endl << logLine << Qt::endl;
    }
    else
    {
        textStream << logLine << Qt::endl;
    }
    outFile.close();
}

bool MainWindow::createDir(QString path)
{
    if (!QDir(path).exists())
    {
        qDebug()  << "Creating stats dir: " << path;
        if (QDir().mkpath(path))
        {
        qDebug()  << "Creating succesfull!";
        }
        else
        {
        qDebug()  << "unable to create: " << path;
        }
    }
    else
    {
        qDebug()  << "Directory alredy exists: " << path;
    }
    if (!QFile(path).exists())
    {
        qDebug() << "imposible to create folder";
        return false;
    }
    else
    {
        return true;
    }
}


