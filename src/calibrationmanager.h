#ifndef CALIBRATIONMANAGER_H
#define CALIBRATIONMANAGER_H

#include <QWidget>
#include <QMap>
#include "calibrationmodel.h"
#include "qcustomplot.h"

class QCPPlottable;
class QMouseEvent;

namespace Ui {
class CalibrationManager;
}

class CalibrationManager : public QWidget
{
    Q_OBJECT

public:
    explicit CalibrationManager(QWidget *parent = nullptr);
    ~CalibrationManager();

    double getCorrection(double frequencyMHz) const;

signals:
    void frequencySelected(double frequencyMHz);
    void currentProfileChanged(const QString &profileName);

public slots:
    void onNewMeasurement(double dbmValue);
    void setActiveProfile(const QString &name);

private slots:
    void ongenerateButton_clicked();
    void onpickAverageButton_clicked();
    void onsaveProfileButton_clicked();
    void onloadProfileButton_clicked();
    void ondeleteProfileButton_clicked();
    void onprofileComboBox_currentIndexChanged(const QString &name);
    void ontable_clicked(const QModelIndex &index);

    void onStartFreqChanged(double value);
    void onEndFreqChanged(double value);
    void onStartUnitChanged(const QString &newUnit);
    void onEndUnitChanged(const QString &newUnit);
    void onStepUnitChanged(const QString &newUnit);

    void updatePlot();
    void onPlotMousePress(QMouseEvent *event);

private:
    void loadProfiles();
    void saveProfile(const QString &name);
    QString getProfilesPath() const;
    void setupPlot();
    void highlightPoint(const QModelIndex &index);

    Ui::CalibrationManager *ui;
    CalibrationModel *m_model;
    QVector<double> m_measurements;
    QModelIndex m_selectedIndex;
    int m_samplesToTake = 0;
    QCPGraph *m_highlightGraph;
};

#endif // CALIBRATIONMANAGER_H
