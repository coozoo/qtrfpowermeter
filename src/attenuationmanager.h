#ifndef ATTENUATIONMANAGER_H
#define ATTENUATIONMANAGER_H

#include <QWidget>
#include <QList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLCDNumber>
#include <QScrollArea>
#include <QLabel>
#include <QDebug>
#include <QFrame>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <limits>
#include "chainsafetyevaluator.h"

class QVBoxLayout;
class QComboBox;
class QLCDNumber;
class AttenuatorWidget;
class QtCoaxCableLossCalcManager;

class AttenuationManager : public QWidget
{
    Q_OBJECT

public:
    explicit AttenuationManager(QWidget *parent = nullptr);
    ~AttenuationManager();
    // deviceMaxInputDbm = absolute-max safe input at the device's front
    // panel (from PMDeviceProperties::maxPowerDbm). Used as the internal
    // stage's rating in the chain-safety check. NaN to skip the check.
    void addInternalAttenuator(double min, double max, double step,
                               double deviceMaxInputDbm = std::numeric_limits<double>::quiet_NaN());
    void removeInternalAttenuator();

    QList<StageInfo> currentStages() const;

signals:
    void totalAttenuationChanged(double totalAttenuation);
    void internalAttenuationChanged(double value);
    void cableManagerAdded(QtCoaxCableLossCalcManager *manager);
    void cableManagerRemoved(QtCoaxCableLossCalcManager *manager);
    // Aggregate signal carrying per-stage incident power, headroom and
    // status. Drives both the calculator's warning surface and each
    // plate's overload visual.
    void safetyStateChanged(ChainReport report);

private slots:
    void addAttenuator();
    void removeSelectedAttenuators();
    void updateTotalAttenuation();

public slots:
    void setInternalAttenuation(double value);
    // Fed by MainWindow when the operating frequency changes. Fans out to
    // every AttenuatorWidget (digital sub-controls use it to pick the right
    // insertion-loss band).
    void setCurrentFrequencyMHz(double freqMHz);
    // Probe input power, fed by TargetPowerCalculator. Triggers chain
    // re-evaluation and safetyStateChanged emission.
    void setProbeInputDbm(double inputDbm);
    // Update the rating used for the pinned internal stage. NaN disables
    // the internal-stage damage check.
    void setDeviceMaxInputDbm(double deviceMaxInputDbm);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void setupUi();
    // Layout-index of the slot just above the drop point. Caller adjusts
    // for self-removal in dropEvent. Returns m_listLayout->count() - 1
    // (i.e. the slot above the stretch) when below all widgets.
    int dropIndexAt(const QPoint &posInScrollContent) const;
    void showInsertIndicatorAt(int layoutIndex);
    void hideInsertIndicator();
    // Clamp so nothing lands at or below the pinned internal widget.
    int clampForInternal(int layoutIndex) const;

    QList<AttenuatorWidget*> m_attenuatorWidgets;
    AttenuatorWidget* m_internalAttenuatorWidget = nullptr;

    // Cached so newly-added widgets get the current frequency at construction.
    // NaN until MainWindow first publishes a value.
    double m_currentFreqHz;
    // Most recent values from the calculator and the active device. Used
    // by reevaluateChain() which runs on any chain-relevant change.
    double m_probeInputDbm;
    double m_deviceMaxInputDbm;
    void reevaluateChain();

    // UI elements
    QVBoxLayout *m_listLayout;
    QComboBox *m_typeComboBox;
    QLCDNumber *m_totalLcd;
    QWidget *m_scrollContent = nullptr;
    QFrame *m_insertIndicator = nullptr;
};

#endif // ATTENUATIONMANAGER_H
