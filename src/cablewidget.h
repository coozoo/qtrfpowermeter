#pragma once

#include <QFrame>
#include "cablemodel.h"

class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;
class QVBoxLayout;

class CableWidget : public QFrame
{
    Q_OBJECT
public:
    explicit CableWidget(CableModel *model, QWidget *parent = nullptr);
    ~CableWidget();

    CableModel *getModel() const;
    double getCurrentAttenuation() const { return m_currentAttenuation; }
    bool isMarkedForDeletion() const;

    // This is the new function to enable/disable per-cable length editing
    void setLengthEditable(bool editable);

public slots:
    void setFrequency(double frequencyMHz);
    void setLength(double lengthM);

signals:
    void attenuationChanged(double newAttenuation);

private slots:
    void updateCalculatedValues();

private:
    void setupUi();
    void populateTable();

    CableModel *m_model;
    double m_currentFrequency = 0.0;
    double m_currentLength = 1.0;
    double m_currentAttenuation = 0.0;

    // UI Elements
    QVBoxLayout *m_mainLayout;
    QCheckBox *m_deleteCheckBox;
    QLineEdit *m_nameLineEdit;
    QTableWidget *m_tableWidget;

    // Pointers to specific items for easy access
    QDoubleSpinBox *m_lengthSpinBox;
    QTableWidgetItem *m_lossValueItem;
};
