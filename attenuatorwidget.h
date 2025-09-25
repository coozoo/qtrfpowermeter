#ifndef ATTENUATORWIDGET_H
#define ATTENUATORWIDGET_H

#include <QGroupBox>
#include <QLabel>
#include <QLCDNumber>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QInputDialog>
#include <QDebug>
#include <QVBoxLayout>
#include <QCheckBox>
#include "qtdigitalattenuator.h"
#include "fixedattenuatorcontrol.h" // Correctly included

class AttenuatorWidget : public QGroupBox
{
    Q_OBJECT

public:
    enum AttenuatorType
    {
        Fixed,
        Digital
    };

    explicit AttenuatorWidget(AttenuatorType type, QWidget *parent = nullptr);
    ~AttenuatorWidget();

    double getAttenuation() const;

    bool isMarkedForRemoval() const { return m_markedForRemoval; }

signals:
    void valueChanged(double newValue);

private slots:
    void onValueChanged(double value);
    void openEditor();

    void onCheckBoxToggled(bool checked);
    void onStatusChanged(bool status);
    void onDescriptionChanged(const QString &description); // Renamed from onModelChanged

private:
    void setupUi();
    void setPressedStyle(bool pressed);

    bool eventFilter(QObject *watched, QEvent *event) override;

    AttenuatorType m_type;
    double m_attenuationValue;
    QString m_description = "";
    bool m_markedForRemoval;
    bool m_editorHasBeenShown; // Flag for initial positioning

    QLabel *m_typeLabel;
    QLabel *m_descrLabel;
    QLCDNumber *m_valueLcd;

    // Pointers to the control/editor windows
    QtDigitalAttenuator *m_digitalControl;
    FixedAttenuatorControl *m_fixedControl;
};

#endif // ATTENUATORWIDGET_H
