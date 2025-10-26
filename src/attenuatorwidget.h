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
#include "fixedattenuatorcontrol.h"
#include "internalattenuatorcontrol.h"

class AttenuatorWidget : public QGroupBox
{
    Q_OBJECT

public:
    enum AttenuatorType
    {
        Fixed,
        Digital,
        InternalDigital
    };

    explicit AttenuatorWidget(AttenuatorType type, QWidget *parent = nullptr);
    ~AttenuatorWidget();

    double getAttenuation() const;

    bool isMarkedForRemoval() const { return m_markedForRemoval; }

    void setInternalProperties(double min, double max, double step);

signals:
    void valueChanged(double newValue);

public slots:
    void setValue(double value);

private slots:
    void onValueChanged(double value);
    void openEditor();

    void onCheckBoxToggled(bool checked);
    void onStatusChanged(bool status);
    void onDescriptionChanged(const QString &description);

private:
    void setupUi();
    void setPressedStyle(bool pressed);

    bool eventFilter(QObject *watched, QEvent *event) override;

    AttenuatorType m_type;
    double m_attenuationValue;
    QString m_description = "";
    bool m_markedForRemoval;
    bool m_editorHasBeenShown;

    QLabel *m_typeLabel;
    QLabel *m_descrLabel;
    QLCDNumber *m_valueLcd;

    QtDigitalAttenuator *m_digitalControl;
    FixedAttenuatorControl *m_fixedControl;
    InternalAttenuatorControl *m_internalControl;
};

#endif // ATTENUATORWIDGET_H
