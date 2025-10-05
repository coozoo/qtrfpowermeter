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

private slots:
    void onUnitChanged();
    void updateCalculations();

private:
    void performHighlighting();
    void updateSpinBoxRange(); // New helper function

    QDoubleSpinBox *m_powerInputSpinBox;
    QComboBox *m_unitComboBox;
    QLabel *m_convertedValueLabel;
    QLabel *m_requiredDbLabel;

    double m_targetDbm;
    double m_minDbm;
    double m_maxDbm;
    double m_actualAttenuation;
    bool m_isUpdating;
    QString m_previousUnit;
};

#endif // TARGETPOWERCALCULATOR_H
