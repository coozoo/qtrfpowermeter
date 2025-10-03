#ifndef CALIBRATIONMANAGER_H
#define CALIBRATIONMANAGER_H

#include <QWidget>
#include <QMap>
#include "calibrationmodel.h"
#include "qcustomplot.h"

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
    void currentProfileChanged(const QString& profileName);

public slots:
    void onNewMeasurement(double dbmValue);
    void setActiveProfile(const QString& name);

private slots:
    void on_generateButton_clicked();
    void on_pickAverageButton_clicked();
    void on_saveProfileButton_clicked();
    void on_loadProfileButton_clicked();
    void on_deleteProfileButton_clicked();
    void on_profileComboBox_currentIndexChanged(const QString &name);
    void on_table_clicked(const QModelIndex &index);

    void onStartFreqChanged(double value);
    void onEndFreqChanged(double value);
    void onStartUnitChanged(const QString &newUnit);
    void onEndUnitChanged(const QString &newUnit);
    void onStepUnitChanged(const QString &newUnit);

    // --- Automatic Plotting ---
    void updatePlot();

private:
    void loadProfiles();
    void saveProfile(const QString& name);
    QString getProfilesPath() const;
    void setupPlot();

    Ui::CalibrationManager *ui;
    CalibrationModel *m_model;
    QVector<double> m_measurements;
    QModelIndex m_selectedIndex;
    int m_samplesToTake = 0;
};

#endif // CALIBRATIONMANAGER_H
