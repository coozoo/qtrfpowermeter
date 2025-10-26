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
#include <QDebug>
#include "serialportinterface.h"
#include "qcustomplot.h"
#include "chartmanager.h"
#include "attenuationmanager.h"
#include "unitconverter.h"
#include "targetpowercalculator.h"
#include "calibrationmanager.h"
#include "pmdevicefactory.h"
#include "abstractpmdevice.h"
#include "devicecomboboxdelegate.h"
#include "qtcoaxcablelosscalcmanager.h"
#include "cablelosscalculatorwindow.h"


QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// Custom roles for storing detailed device info in the QComboBox
enum DeviceInfoRole {
    PortNameRole = Qt::UserRole + 1,
    SystemLocationRole,
    DescriptionRole,
    ManufacturerRole,
    SerialNumberRole,
    VendorIDRole,
    ProductIDRole,
    IsBusyRole
};


enum dataColumns {
    dataTimeColumnID,
    dataValuedBmColumnID,
    dataValuemVppColumnID,
    dataValuemWColumnID,
    dataValueFreqColumnID,
    dataValueCorrectColumnID,
    dataValueAttenuationColumnID,
    dataValueTotalDbmColumnID,
    dataValueTotalMwColumnID
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
    Q_PROPERTY(QString currentDevice
                   READ currentDevice
                       WRITE setCurrentDevice
                           NOTIFY currentDeviceChanged
               )
    Q_PROPERTY(bool isConnected
                   READ isConnected
                       WRITE setIsConnected
                           NOTIFY isConnectedChanged
               )
    Q_PROPERTY(QString deviceError
                   READ deviceError
                       WRITE setDeviceError
                           NOTIFY deviceErrorChanged
               )

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    QStandardItemModel *data_model;
    chartManager *charts;
    QString datetimefile;

    void setFrequency(const QString &m_Frequency)
    {
        curFrequency = m_Frequency;
    }
    const QString &getFrequency() const
    { return curFrequency; }

    void setOffset(const QString &m_Offset)
    {
        curOffset = m_Offset;
    }
    const QString &getOffset() const
    { return curOffset; }

    void setstrDateTimeFile(const QString &m_strDateTimeFile)
    {
        strDateTimeFile = m_strDateTimeFile;
    }
    const QString &getstrDateTimeFile() const
    { return strDateTimeFile; }

    bool createDir(QString path);
    AttenuationManager *attenuationMgr;

    const QString &currentDevice() const
    { return m_currentDevice; }
    void setCurrentDevice(const QString &newCurrentDevice)
    {
        if (m_currentDevice == newCurrentDevice)
            return;
        m_currentDevice = newCurrentDevice;
        qDebug() << "Current device set to:" << m_currentDevice;
        emit currentDeviceChanged();
    }

    bool isConnected() const
    { return m_isConnected; }
    void setIsConnected(bool newIsConnected)
    {
        if (m_isConnected == newIsConnected)
            return;
        m_isConnected = newIsConnected;
        emit isConnectedChanged(m_isConnected);
    }

    const QString &deviceError() const
    { return m_deviceError; }
    void setDeviceError(const QString &newDeviceError)
    {
        if (m_deviceError == newDeviceError)
            return;
        m_deviceError = newDeviceError;
        emit deviceErrorChanged();
    }

private:
    Ui::MainWindow *ui;
    void updateDeviceList();
    QTimer simulatorTimer;
    QString curFrequency="0";
    QString curOffset="0";
    QString strDateTimeFile;
    QString rootstatsdir;
    QString statsdirlocation;
    QString filepath;
    double m_current_atteuation=0;
    TargetPowerCalculator *m_attenuatorCalculator;
    CalibrationManager *m_calibrationManager;

    double m_max_dbm;

    QString m_currentDevice;
    bool m_isConnected = false;
    QString m_deviceError;

    PMDeviceFactory *m_deviceFactory;
    AbstractPMDevice *m_activeDeviceObject = nullptr;
    void setupDeviceSelector();
    void updateUiForDevice(const PMDeviceProperties &props);
    void createDevice(const QString &deviceId);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void ondevice_comboBox_currentIndexChanged();
    void on_connect_pushButton_clicked();
    void on_disconnect_pushButton_clicked();
    void on_resetMax_toolButton_clicked();
    void on_refreshDevices_toolbutton_clicked();
    void on_data_model_rowsInserted(const QModelIndex & parent, int start, int end);
    void on_simulate_checkBox_clicked();
    void on_set_pushButton_clicked();
    void on_simulatorTimer();
    void writeStatCSV(const QString &appendFileName, const QString &logLine, const QString &headersList);
    void onTotalAttenuationChanged(double totalAttenuation);
    void onIsConnectedChanged(bool connected);
    // --- Calibration Slots ---
    void on_calibration_pushButton_toggled(bool checked);
    void onCalibrationFrequencySelected(double frequencyMHz);

    void onDeviceSelector_currentIndexChanged(int index);
    void onDeviceConnected();
    void onDeviceDisconnected();
    void onDeviceError(const QString &error);
    void onNewDeviceMeasurement(double dbm, double vpp_raw);
    void onNewDeviceLogMessage(const QString &message);


    void onDeviceInternalAttChanged(double attDb);

    // Cable Manager Integration Slots
    void onCableManagerAdded(QtCoaxCableLossCalcManager *manager);
    void onCableManagerRemoved(QtCoaxCableLossCalcManager *manager);
    void onCurrentFrequencyChanged(int freqMHz);

    void on_actionCableLossCalculator_triggered();

public slots:
    void on_range_spinBox_valueChanged(int range);
    int on_saveCharts_toolButton_clicked();

signals:
    void newData(QString headersList,QString dataList);
    void newMeasurement(double dbmValue);
    void currentDeviceChanged();
    void isConnectedChanged(bool connected);
    void deviceErrorChanged();
    void currentFrequencyChanged(double freqMHz);
};
#endif // MAINWINDOW_H
