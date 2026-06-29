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
#include "qtdigitalattenuator.h"
#include "fixedattenuatorcontrol.h"
#include "internalattenuatorcontrol.h"
#include "qtcoaxcablelosscalcmanager.h"

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

    bool isMarkedForRemoval() const { return m_markedForRemoval; }

    void setInternalProperties(double min, double max, double step);

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
