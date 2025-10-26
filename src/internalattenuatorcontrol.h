#ifndef INTERNALATTENUATORCONTROL_H
#define INTERNALATTENUATORCONTROL_H

#include "fixedattenuatorcontrol.h"

class InternalAttenuatorControl : public FixedAttenuatorControl
{
    Q_OBJECT
public:
    explicit InternalAttenuatorControl(QWidget *parent = nullptr);
    void setProperties(double min, double max, double step);
};

#endif // INTERNALATTENUATORCONTROL_H
