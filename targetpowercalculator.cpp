#include "targetpowercalculator.h"

TargetPowerCalculator::TargetPowerCalculator(QWidget *parent)
    : QGroupBox(tr("Attenuator Calculator"), parent)
    , m_actualAttenuation(0.0)
    , m_isUpdating(false)
{
    m_targetDbm = std::numeric_limits<double>::quiet_NaN();
    m_minDbm = std::numeric_limits<double>::quiet_NaN();
    m_maxDbm = std::numeric_limits<double>::quiet_NaN();

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    m_powerInputSpinBox = new QDoubleSpinBox();
    m_powerInputSpinBox->setDecimals(8);

    m_unitComboBox = new QComboBox();
    m_unitComboBox->addItems({"dBm", "W", "mW", "µW"});
    m_unitComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_previousUnit = m_unitComboBox->currentText();

    // Set initial state
    updateSpinBoxRange();
    m_powerInputSpinBox->setValue(10.0);

    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->addWidget(new QLabel(tr("Input Power:")));
    inputLayout->addWidget(m_powerInputSpinBox);
    inputLayout->addWidget(m_unitComboBox);

    m_convertedValueLabel = new QLabel(tr("(---)"));
    m_requiredDbLabel = new QLabel(tr("Attenuation Needed: ---"));

    QFont boldFont = m_requiredDbLabel->font();
    boldFont.setBold(true);
    m_requiredDbLabel->setFont(boldFont);

    mainLayout->addLayout(inputLayout);
    mainLayout->addWidget(m_convertedValueLabel);
    mainLayout->addWidget(m_requiredDbLabel);

    connect(m_powerInputSpinBox, SIGNAL(valueChanged(double)), this, SLOT(updateCalculations()));
    connect(m_unitComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(onUnitChanged()));

    updateCalculations();
}

void TargetPowerCalculator::updateSpinBoxRange()
{
    if (m_unitComboBox->currentText() == "dBm")
        {
            m_powerInputSpinBox->setRange(-200.0, 200.0);
        }
    else
        {
            m_powerInputSpinBox->setRange(0.0, 10000.0);
        }
}

void TargetPowerCalculator::setTargetDbm(double targetDbm)
{
    m_targetDbm = targetDbm;

    m_isUpdating = true;
    m_unitComboBox->blockSignals(true);
    m_powerInputSpinBox->blockSignals(true);

    m_unitComboBox->setCurrentText("dBm");
    updateSpinBoxRange(); // Update range before setting value
    m_powerInputSpinBox->setValue(targetDbm);
    m_previousUnit = "dBm";

    m_unitComboBox->blockSignals(false);
    m_powerInputSpinBox->blockSignals(false);
    m_isUpdating = false;

    updateCalculations();
}

void TargetPowerCalculator::setMinDbm(double minDbm)
{
    m_minDbm = minDbm;
    updateCalculations();
}

void TargetPowerCalculator::setMaxDbm(double maxDbm)
{
    m_maxDbm = maxDbm;
    updateCalculations();
}

void TargetPowerCalculator::onUnitChanged()
{
    if (m_isUpdating) return;
    m_isUpdating = true;

    double value = m_powerInputSpinBox->value();
    double watts = 0;

    if (m_previousUnit == "dBm") watts = UnitConverter::dBmToMilliwatts(value) / 1000.0;
    else if (m_previousUnit == "W") watts = value;
    else if (m_previousUnit == "mW") watts = value / 1000.0;
    else if (m_previousUnit == "µW") watts = value / 1000000.0;

    QString currentUnit = m_unitComboBox->currentText();
    double newValue = 0;

    if (currentUnit == "dBm") newValue = (watts > 0) ? UnitConverter::milliwattsToDBm(watts * 1000.0) : -200.0;
    else if (currentUnit == "W") newValue = watts;
    else if (currentUnit == "mW") newValue = watts * 1000.0;
    else if (currentUnit == "µW") newValue = watts * 1000000.0;

    updateSpinBoxRange(); // Update range before setting value

    m_powerInputSpinBox->blockSignals(true);
    m_powerInputSpinBox->setValue(newValue);
    m_powerInputSpinBox->blockSignals(false);

    m_previousUnit = currentUnit;
    m_isUpdating = false;

    updateCalculations();
}

void TargetPowerCalculator::updateCalculations()
{
    if (m_isUpdating) return;

    double value = m_powerInputSpinBox->value();
    QString unit = m_unitComboBox->currentText();
    double input_watts = 0.0;
    double input_dbm = -std::numeric_limits<double>::infinity();

    if (unit == "dBm")
        {
            input_dbm = value;
            input_watts = UnitConverter::dBmToMilliwatts(input_dbm) / 1000.0;
            if (input_watts > 0)
                {
                    QPair<double, QString> formattedPower = UnitConverter::formatPower(input_watts * 1000.0);
                    m_convertedValueLabel->setText(QString("(%1 %2)").arg(formattedPower.first, 0, 'f', 2).arg(formattedPower.second));
                }
            else
                {
                    m_convertedValueLabel->setText(tr("(---)"));
                }
        }
    else
        {
            if (unit == "W") input_watts = value;
            else if (unit == "mW") input_watts = value / 1000.0;
            else if (unit == "µW") input_watts = value / 1000000.0;

            if (input_watts > 0)
                {
                    input_dbm = UnitConverter::milliwattsToDBm(input_watts * 1000.0);
                    m_convertedValueLabel->setText(QString("(%1 dBm)").arg(input_dbm, 0, 'f', 2));
                }
            else
                {
                    m_convertedValueLabel->setText(tr("(---)"));
                }
        }

    if (!std::isnan(m_targetDbm) && input_dbm <= m_targetDbm)
        {
            m_requiredDbLabel->setText(tr("No Need Attenuation (signal is low)"));
        }
    else if (!std::isnan(m_targetDbm) && input_watts > 0)
        {
            double required_attenuation = input_dbm - m_targetDbm;
            m_requiredDbLabel->setText(tr("Attenuation Needed: %1 dB").arg(ceil(required_attenuation)));
        }
    else
        {
            m_requiredDbLabel->setText(tr("Attenuation Needed: ---"));
        }

    performHighlighting();
}

void TargetPowerCalculator::onActualAttenuationChanged(double actualAttenuation)
{
    m_actualAttenuation = actualAttenuation;
    performHighlighting();
}

void TargetPowerCalculator::performHighlighting()
{
    if (std::isnan(m_minDbm) || std::isnan(m_maxDbm))
        {
            m_requiredDbLabel->setStyleSheet("");
            m_requiredDbLabel->setToolTip("");
            return;
        }

    double value = m_powerInputSpinBox->value();
    QString unit = m_unitComboBox->currentText();
    double input_dbm;

    if (unit == "dBm")
        {
            input_dbm = value;
        }
    else
        {
            double watts = 0;
            if (unit == "W") watts = value;
            else if (unit == "mW") watts = value / 1000.0;
            else if (unit == "µW") watts = value / 1000000.0;
            if (watts <= 0)
                {
                    m_requiredDbLabel->setStyleSheet("");
                    m_requiredDbLabel->setToolTip("");
                    return;
                }
            input_dbm = UnitConverter::milliwattsToDBm(watts * 1000.0);
        }

    double signal_at_meter = input_dbm - m_actualAttenuation;

    if (signal_at_meter > m_maxDbm)
        {
            m_requiredDbLabel->setStyleSheet("color: red;");
            m_requiredDbLabel->setToolTip(tr("DANGER: Signal at meter (%1 dBm) is above max (%2 dBm).\nPotential for equipment damage!")
                                          .arg(signal_at_meter, 0, 'f', 1).arg(m_maxDbm, 0, 'f', 1));
        }
    else if (signal_at_meter < m_minDbm)
        {
            m_requiredDbLabel->setStyleSheet("color: blue;");
            m_requiredDbLabel->setToolTip(tr("INFO: Signal at meter (%1 dBm) is below min (%2 dBm).\nMeasurement may be inaccurate or impossible.")
                                          .arg(signal_at_meter, 0, 'f', 1).arg(m_minDbm, 0, 'f', 1));
        }
    else
        {
            m_requiredDbLabel->setStyleSheet("color: green;");
            m_requiredDbLabel->setToolTip(tr("OK: Signal at meter (%1 dBm) is within the safe range [%2, %3] dBm.")
                                          .arg(signal_at_meter, 0, 'f', 1).arg(m_minDbm, 0, 'f', 1).arg(m_maxDbm, 0, 'f', 1));
        }
}
