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

// Forward declarations to keep the header clean
class QVBoxLayout;
class QComboBox;
class QLCDNumber;
class AttenuatorWidget;

class AttenuationManager : public QWidget
{
    Q_OBJECT

public:
    explicit AttenuationManager(QWidget *parent = nullptr);
    ~AttenuationManager();

signals:
    void totalAttenuationChanged(double totalAttenuation);

private slots:
    void addAttenuator();
    void removeSelectedAttenuators();
    void updateTotalAttenuation();

private:
    void setupUi();

    // The list of pointers you correctly suggested
    QList<AttenuatorWidget*> m_attenuatorWidgets;

    // UI elements
    QVBoxLayout *m_listLayout;
    QComboBox *m_typeComboBox;
    QLCDNumber *m_totalLcd;
};

#endif // ATTENUATIONMANAGER_H
