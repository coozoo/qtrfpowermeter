#ifndef TARGETPOWERCALCULATOR_H
#define TARGETPOWERCALCULATOR_H

#include <QGroupBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <cmath>
#include <limits>
#include "unitconverter.h"
#include "chainsafetyevaluator.h"

class TargetPowerCalculator : public QGroupBox
{
    Q_OBJECT

public:
    explicit TargetPowerCalculator(QWidget *parent = nullptr);

    void setTargetDbm(double targetDbm);
    void setMinDbm(double minDbm);
    void setMaxDbm(double maxDbm);

public slots:
    void onActualAttenuationChanged(double actualAttenuation);
    // Fed by AttenuationManager after each chain re-evaluation. Updates the
    // safety label and tooltip.
    void onSafetyState(const ChainReport &report);

signals:
    // Fires whenever the user-entered probe power changes (in dBm). The
    // AttenuationManager listens so it can re-evaluate the chain.
    void inputDbmChanged(double inputDbm);

private slots:
    void onUnitChanged();
    void updateCalculations();

private:
    void performHighlighting();
    void updateSpinBoxRange();

    QDoubleSpinBox *m_powerInputSpinBox;
    QComboBox *m_unitComboBox;
    QLabel *m_convertedValueLabel;
    QLabel *m_requiredDbLabel;
    QLabel *m_safetyLabel;

    double m_targetDbm;
    double m_minDbm;
    double m_maxDbm;
    double m_actualAttenuation;
    bool m_isUpdating;
    QString m_previousUnit;
};

#endif // TARGETPOWERCALCULATOR_H
