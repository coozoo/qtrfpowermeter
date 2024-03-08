#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QSerialPort>
#include <QObject>
#include <QWidget>
#include <QMainWindow>
#include <QSerialPortInfo>
#include <QStandardItemModel>
#include <QScrollBar>
#include <QDateTime>
#include <QTimer>
#include <QRandomGenerator>
#include <QtMath>
#include "serialportinterface.h"
#include "qcustomplot.h"
#include "chartmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

enum dataColumns {
    dataTimeColumnID,
    dataValuedBmColumnID,
    dataValuemVppColumnID,
    dataValuemWColumnID,
    dataValueFreqColumnID,
    dataValueCorrectColumnID
};


class MainWindow : public QMainWindow
{
    Q_OBJECT

    Q_PROPERTY(QString curFrequency
                       READ getFrequency
                       WRITE setFrequency
               )
    Q_PROPERTY(QString curOffset
                   READ getOffset
                       WRITE setOffset
               )

    Q_PROPERTY(QString strDateTimeFile
                   READ getstrDateTimeFile
                       WRITE setstrDateTimeFile
               )

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QStandardItemModel *data_model;
    chartManager *charts;
    QString datetimefile;

    void setFrequency(QString m_Frequency)
    {
        curFrequency = m_Frequency;
    }
    QString getFrequency() const
    { return curFrequency; }

    void setOffset(QString m_Offset)
    {
        curOffset = m_Offset;
    }
    QString getOffset() const
    { return curOffset; }

    void setstrDateTimeFile(QString m_strDateTimeFile)
    {
        strDateTimeFile = m_strDateTimeFile;
    }
    QString getstrDateTimeFile() const
    { return strDateTimeFile; }

    bool createDir(QString path);

private:
    Ui::MainWindow *ui;
    void updateDeviceList();
    SerialPortInterface *serialPortPowerMeter;
    double dBmTomW(double dbm);
    QTimer simulatorTimer;
    QString curFrequency="0";
    QString curOffset="0";
    QString strDateTimeFile;
    QString rootstatsdir;
    QString statsdirlocation;
    QString filepath;

private slots:
    void ondevice_comboBox_currentIndexChanged();
    void updateData(QString data);
    void on_connect_pushButton_clicked();
    void on_disconnect_pushButton_clicked();
    void on_refreshDevices_toolbutton_clicked();
    void on_data_model_rowsInserted(const QModelIndex & parent, int start, int end);
    void on_simulate_checkBox_clicked();
    void on_set_pushButton_clicked();
    void on_simulatorTimer();
    void on_serialPortError(QString error);
    void writeStatCSV(QString appendFileName, QString logLine, QString headersList);

public slots:
    void on_range_spinBox_valueChanged(int range);
    int on_saveCharts_toolButton_clicked();

signals:
    void newData(QString headersList,QString dataList);
};
#endif // MAINWINDOW_H
