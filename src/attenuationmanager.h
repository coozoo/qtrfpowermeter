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
#include <limits>

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
    // Fed by MainWindow when the operating frequency changes. Fans out to
    // every AttenuatorWidget (digital sub-controls use it to pick the right
    // insertion-loss band).
    void setCurrentFrequencyMHz(double freqMHz);

private:
    void setupUi();

    QList<AttenuatorWidget*> m_attenuatorWidgets;
    AttenuatorWidget* m_internalAttenuatorWidget = nullptr;

    // Cached so newly-added widgets get the current frequency at construction.
    // NaN until MainWindow first publishes a value.
    double m_currentFreqHz;

    // UI elements
    QVBoxLayout *m_listLayout;
    QComboBox *m_typeComboBox;
    QLCDNumber *m_totalLcd;
};

#endif // ATTENUATIONMANAGER_H
