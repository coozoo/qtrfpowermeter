#include "internalattenuatorcontrol.h"

InternalAttenuatorControl::InternalAttenuatorControl(QWidget *parent)
    : FixedAttenuatorControl(parent)
{
    setWindowTitle(tr("Device Internal Attenuator Control"));
}

void InternalAttenuatorControl::setProperties(double min, double max, double step)
{
    m_spinBox->setRange(min, max);
    m_spinBox->setSingleStep(step);
}
