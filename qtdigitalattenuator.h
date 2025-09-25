#ifndef QTDIGITALATTENUATOR_H
#define QTDIGITALATTENUATOR_H

#include <QWidget>
#include <QTimer>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDateTime>
#include "serialportinterface.h"
#include "attdevice.h"
#include "unitconverter.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class QtDigitalAttenuator;
}
QT_END_NAMESPACE

class QtDigitalAttenuator : public QWidget
{
    Q_OBJECT

public:
    explicit QtDigitalAttenuator(QWidget *parent = nullptr);
    ~QtDigitalAttenuator();

private:
    Ui::QtDigitalAttenuator *ui;
    void updateDeviceList();
    AttDevice *serialAttenuator;
    QTimer *attenuation_doubleSpinBox_debounceTimer;

private slots:
    void ondevice_comboBox_currentIndexChanged();
    void updateData(const QString &data);
    void on_connect_pushButton_clicked();
    void on_disconnect_pushButton_clicked();
    void on_refreshDevices_toolbutton_clicked();
    void on_serialPortError(QString error);
    void onattenuation_doubleSpinBox_valueChanged(double value);
    void on_set_pushButton_clicked();
    void on_send_pushButton_clicked();
    void on_currentAttenuation_changed(double value);
    void ondetectedDevice(const QString &model, double step, double max, const QString &format);
    void ondeviceConsole_pushButton_clicked();
    void ondeviceSetStatus(bool status);
signals:
    void currentValueChanged(double value);
    void valueSetStatus(bool status);
    void modelChanged(const QString &model);
};

#endif // QTDIGITALATTENUATOR_H
