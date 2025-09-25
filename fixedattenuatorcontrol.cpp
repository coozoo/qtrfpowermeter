#include "fixedattenuatorcontrol.h"

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

    m_descriptionEdit = new QLineEdit(this);
    connect(m_descriptionEdit, &QLineEdit::editingFinished, this, &FixedAttenuatorControl::onDescriptionEditingFinished);

    layout->addRow(tr("Attenuation:"), m_spinBox);
    layout->addRow(tr("Description:"), m_descriptionEdit);

    setLayout(layout);
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
