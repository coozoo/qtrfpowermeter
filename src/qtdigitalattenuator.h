#ifndef QTDIGITALATTENUATOR_H
#define QTDIGITALATTENUATOR_H

#include <QWidget>
#include <QTimer>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QTableWidget>
#include <QLabel>
#include <QGroupBox>
#include <limits>
#include "serialportinterface.h"
#include "attdevice.h"
#include "unitconverter.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class QtDigitalAttenuator;
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

class QtDigitalAttenuator : public QWidget
{
    Q_OBJECT

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
    explicit QtDigitalAttenuator(QWidget *parent = nullptr);
    ~QtDigitalAttenuator();

    const QString &currentDevice() const
    {return m_currentDevice;}
    void setCurrentDevice(const QString &newCurrentDevice)
    {
        if (m_currentDevice == newCurrentDevice)
            return;
        m_currentDevice = newCurrentDevice;
        emit currentDeviceChanged();
    }

    bool isConnected() const
    {return m_isConnected;}
    void setIsConnected(bool newIsConnected)
    {
        if (m_isConnected == newIsConnected)
            return;
        m_isConnected = newIsConnected;
        emit isConnectedChanged(m_isConnected);
    }

    const QString &deviceError() const
    {return m_deviceError;}
    void setDeviceError(const QString &newDeviceError)
    {
        if (m_deviceError == newDeviceError)
            return;
        m_deviceError = newDeviceError;
        emit deviceErrorChanged();
    }

public:
    // Chip-level absolute-max CW input rating for the detected model. NaN
    // before a known device is detected (e.g. unsupported boards).
    double maxInputDbm() const { return m_maxInputDbm; }
    const QString &chipName() const { return m_chip; }

public slots:
    // Operating frequency feed for IL lookup. Hz. NaN means "unknown" and
    // the IL table renders no highlighted band.
    void setCurrentFrequencyHz(double freqHz);

private:
    Ui::QtDigitalAttenuator *ui;
    void updateDeviceList();
    void setupInsertionLossWidgets();
    void rebuildIlTableForBands(const QList<InsertionLossBand> &bands);
    void refreshIlHighlight();
    void emitEffective();
    AttDevice *serialAttenuator;
    QTimer *attenuation_doubleSpinBox_debounceTimer;
    QString m_currentDevice;
    bool m_isConnected = false;
    QString m_deviceError;
    double m_maxInputDbm = std::numeric_limits<double>::quiet_NaN();
    QString m_chip;
    double m_currentFreqHz = std::numeric_limits<double>::quiet_NaN();
    double m_currentIlDb = 0.0;
    QGroupBox *m_ilGroupBox = nullptr;
    QTableWidget *m_ilTable = nullptr;
    QLabel *m_effectiveLabel = nullptr;

private slots:
    void onPortOpened();
    void onPortClosed();
    void onIsConnectedChanged(bool connected);
    void ondevice_comboBox_currentIndexChanged();
    void updateData(const QString &data);
    void on_connect_pushButton_clicked();
    void on_disconnect_pushButton_clicked();
    void on_refreshDevices_toolbutton_clicked();
    void on_serialPortError(const QString &error);
    void onattenuation_doubleSpinBox_valueChanged(double value);
    void on_set_pushButton_clicked();
    void on_send_pushButton_clicked();
    void on_currentAttenuation_changed(double value);
    void ondetectedDevice(const QString &model, double step, double max, const QString &format,
                          double maxInputDbm, const QString &chip);
    void ondeviceConsole_pushButton_clicked();
    void ondeviceSetStatus(bool status);
    void on_useHardButtons_checkBox_stateChanged(int state);

signals:
    void currentValueChanged(double value);
    void valueSetStatus(bool status);
    void modelChanged(const QString &model);
    // Emitted when a known model is detected, carrying its chip-level
    // CW input rating. NaN means unknown / unsupported board.
    void maxInputDbmChanged(double maxInputDbm, const QString &chip);
    // Fires whenever nominal attenuation or the current operating frequency
    // changes. effectiveDb = nominalDb + ilDb (out-of-band: ilDb = 0).
    void effectiveAttenuationChanged(double nominalDb, double ilDb, double effectiveDb);
    void currentDeviceChanged();
    void isConnectedChanged(bool connected);
    void deviceErrorChanged();
};

#endif // QTDIGITALATTENUATOR_H
