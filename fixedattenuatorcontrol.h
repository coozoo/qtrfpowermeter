#ifndef FIXEDATTENUATORCONTROL_H
#define FIXEDATTENUATORCONTROL_H

#include <QWidget>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QFormLayout>
#include <QPair>
#include "unitconverter.h"
#include <QtMath>

class FixedAttenuatorControl : public QWidget
{
    Q_OBJECT

public:
    explicit FixedAttenuatorControl(QWidget *parent = nullptr);

    void setValue(double value);
    void setDescription(const QString &description);

signals:
    void valueChanged(double newValue);
    void descriptionChanged(const QString &newDescription);

private slots:
    void onSpinBoxValueChanged(double value);
    void onDescriptionEditingFinished();

private:
    void setupUi();

    QDoubleSpinBox *m_spinBox;
    QLineEdit *m_descriptionEdit;
};

#endif // FIXEDATTENUATORCONTROL_H
