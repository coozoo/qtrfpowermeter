#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    updateDeviceList();
    serialPortReader= new SerialPortReader();
    connect(ui->device_comboBox, &QComboBox::currentTextChanged, this, &MainWindow::ondevice_comboBox_currentIndexChanged);
    connect(serialPortReader,&SerialPortReader::serialPortNewData,this,&MainWindow::updateData);

    data_model=new QStandardItemModel(0,0,this);
    data_model->setHorizontalHeaderItem(dataTimeColumnID,new QStandardItem(QString("Time")));
    data_model->setHorizontalHeaderItem(dataValuedBmColumnID,new QStandardItem(QString("dBm")));
    data_model->setHorizontalHeaderItem(dataValuemVppColumnID,new QStandardItem(QString("mVpp")));
    data_model->setHorizontalHeaderItem(dataValuemWColumnID,new QStandardItem(QString("mW")));
    data_model->setHorizontalHeaderItem(dataValueFreqColumnID,new QStandardItem(QString("Frequency MHz")));
    data_model->setHorizontalHeaderItem(dataValueCorrectColumnID,new QStandardItem(QString("Correction dB")));

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

    //set datetime stamp for filename
    datetimefile = QDateTime::fromMSecsSinceEpoch(QDateTime::currentMSecsSinceEpoch()).toString("dd.MM.yyyy_hh.mm.ss.zzz");

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
    connect(this, SIGNAL(newData(QString, QString)), charts, SLOT(dataIncome(QString, QString)));
    Q_EMIT(on_set_pushButton_clicked());
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::ondevice_comboBox_currentIndexChanged()
{
    qDebug()<<ui->device_comboBox->currentData().toString();
}

void MainWindow::updateData(QString data)
{
    QString curdate=QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss.zzz");
    ui->data_plainTextEdit->appendPlainText(curdate+" "+data);

    QRegularExpression reg("[$]([-+][0-9.]+).+dBm([0-9.]+).+(.)Vpp[$]");

    QRegularExpressionMatchIterator i = reg.globalMatch(data);
    if (i.isValid())
    {
        while (i.hasNext())
        {
            QRegularExpressionMatch match = i.next();
            qDebug()<<match.captured(1);
            ui->dbm_lcdNumber->display(match.captured(1));
            bool ok;
            double mw=0;
            mw=dBmTomW(match.captured(1).toDouble(&ok));
            if(ok)
            {
                ui->mW_lcdNumber->display(mw);
                if(ui->maxmw_lineEdit->text().toDouble()<mw)
                {
                    ui->maxmw_lineEdit->setText(QString::number(mw,'f',4));
                    ui->maxdbm_lineEdit->setText(match.captured(1));
                }
            }
            else
            {
                ui->mW_lcdNumber->display("Error");
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
        }
    }
}

void MainWindow::on_connect_pushButton_clicked()
{
    qDebug()<<"on_connect_pushButton_clicked()";

        if(ui->device_comboBox->currentData().toString().isEmpty())
        {
            serialPortReader->setportName(ui->device_comboBox->currentData().toString());
            serialPortReader->setbaudRate(9600);
            serialPortReader->startPort();
        }

}

void MainWindow::on_refresh_toolButton_clicked()
{
    updateDeviceList();
}

void MainWindow::updateDeviceList()
{
    ui->device_comboBox->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos)
    {
        qDebug()<<info.hasVendorIdentifier() <<QString::number(info.vendorIdentifier());
        if(info.hasVendorIdentifier() && QString::number(info.vendorIdentifier(),16)=="1a86")
        {
            QString s = tr("Port: ") + info.portName() +
                        tr(" Location: ") + info.systemLocation() +
                        tr("; Description: ") + info.description() +
                        tr("; Manufacturer: ") + info.manufacturer() +
                        tr("; S\\n: ") + info.serialNumber() +
                        tr("; VId: ") + (info.hasVendorIdentifier() ? QString::number(info.vendorIdentifier(), 16) : QString()) +
                        tr("; PId: ") + (info.hasProductIdentifier() ? QString::number(info.productIdentifier(), 16) : QString()) +
                        (info.isBusy()?QString(" [Busy]"):QString(""));
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
    qDebug()<<"simulator in progress";
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
    setFrequency(QString::number(ui->frequency_spinBox->value(),'d',0));
    //if(ui->correctionplus_radioButton->isChecked())
    //{
    setOffset(QString(ui->correctionplus_radioButton->isChecked()?"+":"-")+QString::number(ui->offset_doubleSpinBox->value(),'f',1));
    //}
    qDebug()<<getFrequency();
    qDebug()<<getOffset();
}
