#include "fixedattenuatorcontrol.h"
#include <QHBoxLayout>
#include <cmath>
#include <limits>

FixedAttenuatorControl::FixedAttenuatorControl(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setWindowTitle(tr("Fixed Attenuator Control"));
}

void FixedAttenuatorControl::setupUi()
{
    QFormLayout *layout = new QFormLayout(this);
    layout->setSpacing(10);

    m_spinBox = new QDoubleSpinBox(this);
    m_spinBox->setRange(0, 100);
    m_spinBox->setDecimals(2);
    m_spinBox->setSingleStep(0.1);
    m_spinBox->setSuffix(" dB");
    connect(m_spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FixedAttenuatorControl::onSpinBoxValueChanged);

    // --- Max-input rating: spinbox + unit combo, canonical in dBm. ---
    m_maxInputSpinBox = new QDoubleSpinBox(this);
    m_maxInputSpinBox->setDecimals(3);
    m_maxInputUnitCombo = new QComboBox(this);
    m_maxInputUnitCombo->addItems({ "W", "mW", "dBm" });
    m_maxInputUnitCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_previousMaxInputUnit = m_maxInputUnitCombo->currentText();
    // Range + initial display derive from m_maxInputDbm (default 1 W).
    refreshMaxInputDisplay();

    connect(m_maxInputSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &FixedAttenuatorControl::onMaxInputSpinBoxChanged);
    connect(m_maxInputUnitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FixedAttenuatorControl::onMaxInputUnitChanged);

    QWidget *maxInputRow = new QWidget(this);
    QHBoxLayout *maxInputLayout = new QHBoxLayout(maxInputRow);
    maxInputLayout->setContentsMargins(0, 0, 0, 0);
    maxInputLayout->addWidget(m_maxInputSpinBox, 1);
    maxInputLayout->addWidget(m_maxInputUnitCombo, 0);

    m_descriptionEdit = new QLineEdit(this);
    connect(m_descriptionEdit, &QLineEdit::editingFinished, this, &FixedAttenuatorControl::onDescriptionEditingFinished);

    layout->addRow(tr("Attenuation:"), m_spinBox);
    layout->addRow(tr("Max Input:"), maxInputRow);
    layout->addRow(tr("Description:"), m_descriptionEdit);

    setLayout(layout);
}

double FixedAttenuatorControl::currentMaxInputAsDbm() const
{
    const double value = m_maxInputSpinBox->value();
    const QString unit = m_maxInputUnitCombo->currentText();
    if (unit == QStringLiteral("dBm")) return value;
    double mw = 0.0;
    if (unit == QStringLiteral("W"))  mw = value * 1000.0;
    else if (unit == QStringLiteral("mW")) mw = value;
    return (mw > 0.0) ? UnitConverter::milliwattsToDBm(mw)
                      : -std::numeric_limits<double>::infinity();
}

void FixedAttenuatorControl::refreshMaxInputDisplay()
{
    m_maxInputUpdating = true;
    const QString unit = m_maxInputUnitCombo->currentText();
    double display = 0.0;
    if (unit == QStringLiteral("dBm")) {
        m_maxInputSpinBox->setRange(-30.0, 60.0);
        m_maxInputSpinBox->setSuffix(QStringLiteral(" dBm"));
        m_maxInputSpinBox->setSingleStep(0.5);
        display = m_maxInputDbm;
    } else if (unit == QStringLiteral("W")) {
        m_maxInputSpinBox->setRange(0.001, 1000.0);
        m_maxInputSpinBox->setSuffix(QStringLiteral(" W"));
        m_maxInputSpinBox->setSingleStep(0.1);
        display = UnitConverter::dBmToMilliwatts(m_maxInputDbm) / 1000.0;
    } else { // mW
        m_maxInputSpinBox->setRange(0.001, 1000000.0);
        m_maxInputSpinBox->setSuffix(QStringLiteral(" mW"));
        m_maxInputSpinBox->setSingleStep(1.0);
        display = UnitConverter::dBmToMilliwatts(m_maxInputDbm);
    }
    m_maxInputSpinBox->setValue(display);
    m_maxInputUpdating = false;
}

void FixedAttenuatorControl::onMaxInputSpinBoxChanged(double /*value*/)
{
    if (m_maxInputUpdating) return;
    const double dbm = currentMaxInputAsDbm();
    if (std::isinf(dbm) || std::isnan(dbm)) return;
    if (qFuzzyCompare(dbm + 1.0, m_maxInputDbm + 1.0)) return;
    m_maxInputDbm = dbm;
    emit maxInputDbmChanged(m_maxInputDbm);
}

void FixedAttenuatorControl::onMaxInputUnitChanged()
{
    if (m_maxInputUpdating) return;
    // Preserve canonical dBm across unit switches by re-rendering from
    // m_maxInputDbm rather than from the spinbox's raw number.
    refreshMaxInputDisplay();
    m_previousMaxInputUnit = m_maxInputUnitCombo->currentText();
}

void FixedAttenuatorControl::setMaxInputDbm(double dbm)
{
    if (qFuzzyCompare(dbm + 1.0, m_maxInputDbm + 1.0)) return;
    m_maxInputDbm = dbm;
    refreshMaxInputDisplay();
    emit maxInputDbmChanged(m_maxInputDbm);
}

void FixedAttenuatorControl::setValue(double value)
{
    // Block signals to prevent a loop when the editor is opened
    bool wasBlocked = m_spinBox->blockSignals(true);
    m_spinBox->setValue(value);
    onSpinBoxValueChanged(value); // Manually update the wattage label to reflect the initial value
    m_spinBox->blockSignals(wasBlocked);
}

void FixedAttenuatorControl::setDescription(const QString &description)
{
    m_descriptionEdit->setText(description);
}

void FixedAttenuatorControl::onSpinBoxValueChanged(double value)
{
    emit valueChanged(value);
}

void FixedAttenuatorControl::onDescriptionEditingFinished()
{
    emit descriptionChanged(m_descriptionEdit->text());
}
