#ifndef FIXEDATTENUATORCONTROL_H
#define FIXEDATTENUATORCONTROL_H

#include <QWidget>
#include <QDoubleSpinBox>
#include <QComboBox>
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
    // Max safe RF input, stored canonically in dBm. Default 30 dBm (1 W).
    double maxInputDbm() const { return m_maxInputDbm; }
    void setMaxInputDbm(double dbm);

signals:
    void valueChanged(double newValue);
    void descriptionChanged(const QString &newDescription);
    void maxInputDbmChanged(double newDbm);

private slots:
    void onSpinBoxValueChanged(double value);
    void onDescriptionEditingFinished();
    void onMaxInputSpinBoxChanged(double value);
    void onMaxInputUnitChanged();

private:
    void setupUi();
    // Re-range the spinbox and re-display m_maxInputDbm using the unit
    // currently selected in m_maxInputUnitCombo. Caller is responsible for
    // setting m_previousMaxInputUnit afterwards.
    void refreshMaxInputDisplay();
    double currentMaxInputAsDbm() const;

protected:
    QDoubleSpinBox *m_spinBox;
    QDoubleSpinBox *m_maxInputSpinBox = nullptr;
    QComboBox *m_maxInputUnitCombo = nullptr;
    QLineEdit *m_descriptionEdit;
    // Canonical rating. Updated via the spinbox or setMaxInputDbm(); read
    // by the chain-safety evaluator through AttenuatorWidget::maxInputDbm.
    double m_maxInputDbm = 30.0; // 1 W
    QString m_previousMaxInputUnit;
    bool m_maxInputUpdating = false;
};

#endif // FIXEDATTENUATORCONTROL_H
