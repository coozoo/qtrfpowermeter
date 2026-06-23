#ifndef CALIBRATIONMANAGER_H
#define CALIBRATIONMANAGER_H

#include <QWidget>
#include <QMap>
#include <QQueue>
#include <QSettings>
#include "calibrationmodel.h"
#include "qcustomplot.h"

class QCPPlottable;
class QMouseEvent;
class TinySaSourceController;
class ConceptRfRpmDevice;
class DeviceCalibrationViewerWidget;
class AdvancedCalibrationTableModel;
class QTableView;
class QDoubleSpinBox;
class QSpinBox;
class QToolButton;
class QTimer;

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
    // Mode-aware overload. MainWindow apply path calls this so Advanced
    // can do bilinear interp on the freshly-measured value. Simple
    // ignores `measuredDbm`, Disabled returns 0. See CalibrationModel.
    double getCorrection(double frequencyMHz, double measuredDbm) const;
    void loadSettings();

    // Per-device persistence anchor. MainWindow passes the active
    // deviceId here whenever the device changes. The manager loads the
    // device's last-used mode + auto state from QSettings under
    // `CalibrationManager/<deviceId>/...` and writes back on any change.
    void setActiveDeviceId(const QString &deviceId);

    // ConceptRF lock. Non-null device: lock mode to Disabled (radios
    // greyed), hide the user-calibration body, show the device's factory
    // calibration table inline. Caller must guarantee the device is in
    // Ready state so its lookup table is stable. Pass nullptr to release
    // the lock (e.g. on disconnect or device-change).
    void setActiveConceptRfDevice(const ConceptRfRpmDevice *device);

signals:
    void frequencySelected(double frequencyMHz);
    void currentProfileChanged(const QString &profileName);

public slots:
    void onNewMeasurement(double dbmValue);
    void setActiveProfile(const QString &name);
    void onDeviceConnectionStateChanged(bool connected);

private slots:
    void ongenerateButton_clicked();
    void onpickAverageButton_clicked();
    void onsaveProfileButton_clicked();
    void onloadProfileButton_clicked();
    void ondeleteProfileButton_clicked();
    void onprofileComboBox_currentIndexChanged(const QString &name);
    void ontable_clicked(const QModelIndex &index);

    // Mode / Auto / TinySa wiring (5e).
    void onModeChanged();
    // Advanced power-axis Apply button (5f).
    void onAdvancedAxisApplyClicked();
    // Per-cell watchdog: if a dispatched cell goes silent (firmware
    // accepted the freq but no signal arrives at the meter), skip it
    // and dispatch the next one rather than wedging the queue.
    void onAutoCellTimeout();
    void onAutoCheckBoxToggled(bool checked);
    void onTinySaRefreshClicked();
    void onTinySaConnectClicked();
    void onTinySaConnected();
    void onTinySaDisconnected();
    void onTinySaError(const QString &msg);
    void onTinySaOutputSet(double freqHz, double levelDbm);
    void onTinySaOutputClamped(double freqHz, double askedDbm, double predictedDbm);
    void onCalibrateAllClicked();

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

    // Mode + Auto state helpers.
    void applyModeUi(CalibrationModel::Mode mode);
    void persistMode(CalibrationModel::Mode mode);
    void persistAuto(bool autoOn);
    void refreshTinySaPorts();
    QString settingsKeyFor(const QString &leaf) const;
    void loadPersistedModeAndAuto();
    void syncAdvancedAxisSpinsFromModel();

    // Auto-fill driving:  enqueue the next unset row, set TinySa, take
    // samples, store, advance.  Single-row triggered by Calibrate Selected
    // when Auto is on; Calibrate All loads every unset row into the queue.
    void startAutoCellForCurrentSelection();
    void dispatchNextAutoCell();
    void onAutoSampleArrived(double dbmValue);

    Ui::CalibrationManager *ui;
    CalibrationModel *m_model;
    QVector<double> m_measurements;
    QModelIndex m_selectedIndex;
    int m_samplesToTake = 0;
    QCPGraph *m_highlightGraph;
    // Advanced-mode plot: freq vs correction at the currently selected
    // column (level). Lives on the shared plotWidget; hidden when the
    // active mode is Simple.
    QCPGraph *m_advancedColumnGraph = nullptr;

    // TinySa source for auto calibration. Owned by the manager; created
    // lazily on first Connect click.
    TinySaSourceController *m_tinySa = nullptr;
    bool m_tinySaConnected = false;

    // Per-device persistence anchor; empty string means "global default".
    QString m_activeDeviceId;

    // ConceptRF lock + embedded factory-table widget. Lazy-created when
    // the lock is first applied; hidden otherwise.
    DeviceCalibrationViewerWidget *m_factoryView = nullptr;
    bool m_conceptLocked = false;

    // Advanced-mode editor (5f). Created in the constructor next to the
    // Simple QTableView. Mode change toggles which one is visible.
    AdvancedCalibrationTableModel *m_advancedAdapter = nullptr;
    QTableView      *m_advancedTableView    = nullptr;
    QWidget         *m_advancedAxisRow      = nullptr;
    QDoubleSpinBox  *m_advancedAxisMinSpin  = nullptr;
    QDoubleSpinBox  *m_advancedAxisMaxSpin  = nullptr;
    QDoubleSpinBox  *m_advancedAxisStepSpin = nullptr;
    QToolButton     *m_advancedAxisApplyBtn = nullptr;

    // Per-cell watchdog: if a dispatched cell goes silent (firmware
    // accepted the freq but the meter never produces samples), skip it
    // and dispatch the next one rather than wedging the queue.
    QTimer   *m_autoWatchdog = nullptr;

    // Auto-fill queue. For Simple sweeps each entry is a row index
    // into m_model->getPoints() (col stays -1). For Advanced sweeps
    // each entry is a (rowIdx, colIdx) pair into advancedRows() /
    // advancedPowerAxis() with col >= 0.
    QQueue<QPair<int,int>> m_autoQueue;
    int m_autoCurrentRow = -1;
    int m_autoCurrentCol = -1;
    bool m_autoSequence = false; // true when Calibrate All is running
    // Snapshot of which mode the active sweep started in. Used so a
    // mid-sweep mode flip doesn't cross the dispatch / completion paths.
    CalibrationModel::Mode m_autoMode = CalibrationModel::Mode::Simple;

    // Level we actually asked the TinySa to output for the current cell.
    // Differs from the user-asked refPower when the asked level was
    // outside the freq's envelope: we clamp to (max - 5 dB) or (min + 5
    // dB) so the generator drives a known-achievable level, and we feed
    // that level (not the user-asked refPower) into the correction
    // formula. Mirrored into the refPower spinbox while the cell is in
    // flight so the user can see what is being driven.
    double m_autoCellRefDbm = 0.0;
    // Snapshot of the user-asked refPower at the start of an auto run.
    // dispatchLevelFor uses this as the source-of-truth "asked" so the
    // mirrored spinbox value does not feed back as the next cell's ask.
    // Restored to the spinbox when the run ends.
    double m_autoAskedRefDbm = 0.0;

    // Helper: compute the level we actually use for a given freq.
    double dispatchLevelFor(double freqHz, double askedDbm) const;
    // Helper: snapshot user-asked refPower and remember it; mirror a
    // value into the spinbox without re-firing valueChanged.
    void   mirrorRefSpinbox(double dbm);

    // Advanced auto-fill helpers (5f).
    void enqueueAdvancedSweep();
    void dispatchNextAdvancedCell();
    // Redraws the Advanced freq-vs-correction-at-column curve on the
    // shared plotWidget. Reads the currently selected column from
    // m_advancedTableView->currentIndex().
    void updateAdvancedColumnChart();
    // Aborts an in-flight Calibrate All sweep: clears the queue,
    // stops the watchdog, restores refPower, resets the button text.
    void cancelAutoSweep();

    // Advanced single-cell manual calibration. Snapshot of the
    // (row, col) chosen at the moment Calibrate Selected was clicked
    // in Advanced mode, so a later selection change in the grid
    // doesn't redirect the samples mid-collection. -1 = inactive.
    int m_manualAdvancedRow = -1;
    int m_manualAdvancedCol = -1;
};

#endif // CALIBRATIONMANAGER_H
