#ifndef DEVICECALIBRATIONVIEWERWIDGET_H
#define DEVICECALIBRATIONVIEWERWIDGET_H

#include <QWidget>
#include <QVector>
#include <QString>

class ConceptRfRpmDevice;
class QStandardItemModel;
class QCPGraph;

namespace Ui {
class DeviceCalibrationViewerWidget;
}

// Read-only view of a ConceptRF device's factory calibration table.
// Reusable embeddable widget: the standalone dialog wraps this widget,
// and CalibrationManager embeds it inline for ConceptRF.
//
// Caller must guarantee the device has reached Ready (no further writes
// to the lookup table) before calling setDevice(). nullptr clears.
class DeviceCalibrationViewerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceCalibrationViewerWidget(QWidget *parent = nullptr);
    ~DeviceCalibrationViewerWidget() override;

    void setDevice(const ConceptRfRpmDevice *device);

private slots:
    void onRowSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void onExportCsvClicked();

private:
    void clear();
    void populateTable();
    void setupPlot();
    void plotRow(int freqIndex);
    QString formatFreq(quint64 hz) const;

    Ui::DeviceCalibrationViewerWidget *ui;
    QStandardItemModel *m_tableModel;
    QCPGraph *m_curve;

    QString m_modelName;
    QString m_firmwareVersion;
    QString m_serialNumber;
    QVector<quint64> m_freqAxisHz;
    QVector<float>   m_powerAxisDb;
    QVector<QVector<float>> m_voltageTableMv;
};

#endif
