#ifndef ATTENUATORWIDGET_H
#define ATTENUATORWIDGET_H

#include <QGroupBox>
#include <QLabel>
#include <QLCDNumber>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QInputDialog>
#include <QDebug>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QPoint>
#include <limits>
#include "qtdigitalattenuator.h"
#include "fixedattenuatorcontrol.h"
#include "internalattenuatorcontrol.h"
#include "qtcoaxcablelosscalcmanager.h"
#include "chainsafetyevaluator.h"

class AttenuatorWidget : public QGroupBox
{
    Q_OBJECT

public:
    enum AttenuatorType
    {
        Fixed,
        Digital,
        InternalDigital,
        Cable
    };

    explicit AttenuatorWidget(AttenuatorType type, QWidget *parent = nullptr);
    ~AttenuatorWidget();

    // Effective attenuation: nominal + insertion loss for the current
    // operating frequency (digital only; equals nominal for fixed, cable,
    // internal). This is what AttenuationManager sums into the total.
    double getAttenuation() const;

    // Chip / model max CW input rating for this stage. NaN means unknown
    // (fixed attenuators without a user-set rating, cables, unsupported
    // digital boards). The chain evaluator treats NaN as "do not check".
    double maxInputDbm() const;

    // Human-readable label for this stage, used in safety warnings.
    QString descriptionText() const { return m_description; }

    // Driven by AttenuationManager after each chain re-evaluation. Updates
    // the plate's border and LCD background plus a tooltip describing
    // how much headroom is left (or by how much the stage is over).
    void setOverloadState(const StageReport &report);

    bool isMarkedForRemoval() const { return m_markedForRemoval; }

    void setInternalProperties(double min, double max, double step,
                               double deviceMaxInputDbm = std::numeric_limits<double>::quiet_NaN());

    QtCoaxCableLossCalcManager *cableManager() const { return m_cableManager; }

signals:
    void valueChanged(double newValue);
    void maxInputDbmChanged(double maxInputDbm);

public slots:
    void setValue(double value);
    // Fed by AttenuationManager. Forwarded to the digital sub-control so its
    // insertion-loss table picks the right band.
    void setCurrentFrequencyHz(double freqHz);

private slots:
    void onValueChanged(double value);
    void onEffectiveChanged(double nominalDb, double ilDb, double effectiveDb);
    void openEditor();

    void onCheckBoxToggled(bool checked);
    void onStatusChanged(bool status);
    void onDescriptionChanged(const QString &description);

private:
    void setupUi();
    void setPressedStyle(bool pressed);

    bool eventFilter(QObject *watched, QEvent *event) override;

    AttenuatorType m_type;
    // Device-front-panel rating for the pinned internal stage. NaN for
    // every other type; the manager skips the check when NaN.
    double m_internalDeviceMaxInputDbm = std::numeric_limits<double>::quiet_NaN();
    // Last overload report applied to the plate. Tracked so re-evaluations
    // that produce the same visible state (status + the numbers we render
    // into the tooltip) skip setStyleSheet/setToolTip and avoid N-plate
    // style-recalc storms on every spinbox keystroke.
    StageStatus m_overloadStatus = StageStatus::Ok;
    double m_lastOverloadIncidentDbm = std::numeric_limits<double>::quiet_NaN();
    double m_lastOverloadRatedDbm = std::numeric_limits<double>::quiet_NaN();
    bool m_overloadStateApplied = false;
    // Drag-source tracking. Negative position means "no press in progress"
    // (we ignore the press if it landed on the checkbox or on an
    // InternalDigital plate, both of which never start a drag).
    QPoint m_dragStartPos { -1, -1 };
    // For digital this tracks the device's nominal value (what the LCD
    // shows); for everything else it equals the effective value.
    double m_attenuationValue;
    // Sum of nominal + IL. Equal to m_attenuationValue for non-digital
    // widgets. This is what getAttenuation() returns.
    double m_effectiveValue;
    QString m_description = "";
    bool m_markedForRemoval;
    bool m_editorHasBeenShown;

    QLabel *m_typeLabel;
    QLabel *m_descrLabel;
    QLCDNumber *m_valueLcd;

    QtDigitalAttenuator *m_digitalControl;
    FixedAttenuatorControl *m_fixedControl;
    InternalAttenuatorControl *m_internalControl;
    QtCoaxCableLossCalcManager *m_cableManager;
};

#endif // ATTENUATORWIDGET_H
