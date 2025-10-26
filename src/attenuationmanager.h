#ifndef ATTENUATIONMANAGER_H
#define ATTENUATIONMANAGER_H

#include <QWidget>
#include <QList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLCDNumber>
#include <QScrollArea>
#include <QLabel>
#include <QDebug>

class QVBoxLayout;
class QComboBox;
class QLCDNumber;
class AttenuatorWidget;
class QtCoaxCableLossCalcManager;

class AttenuationManager : public QWidget
{
    Q_OBJECT

public:
    explicit AttenuationManager(QWidget *parent = nullptr);
    ~AttenuationManager();
    void addInternalAttenuator(double min, double max, double step);
    void removeInternalAttenuator();

signals:
    void totalAttenuationChanged(double totalAttenuation);
    void internalAttenuationChanged(double value);
    void cableManagerAdded(QtCoaxCableLossCalcManager *manager);
    void cableManagerRemoved(QtCoaxCableLossCalcManager *manager);

private slots:
    void addAttenuator();
    void removeSelectedAttenuators();
    void updateTotalAttenuation();

public slots:
    void setInternalAttenuation(double value);

private:
    void setupUi();

    QList<AttenuatorWidget*> m_attenuatorWidgets;
    AttenuatorWidget* m_internalAttenuatorWidget = nullptr;

    // UI elements
    QVBoxLayout *m_listLayout;
    QComboBox *m_typeComboBox;
    QLCDNumber *m_totalLcd;
};

#endif // ATTENUATIONMANAGER_H
