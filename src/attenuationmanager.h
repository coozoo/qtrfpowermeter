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
#include <QFrame>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
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

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void setupUi();
    // Layout-index of the slot just above the drop point. Caller adjusts
    // for self-removal in dropEvent. Returns m_listLayout->count() - 1
    // (i.e. the slot above the stretch) when below all widgets.
    int dropIndexAt(const QPoint &posInScrollContent) const;
    void showInsertIndicatorAt(int layoutIndex);
    void hideInsertIndicator();
    // Clamp so nothing lands at or below the pinned internal widget.
    int clampForInternal(int layoutIndex) const;

    QList<AttenuatorWidget*> m_attenuatorWidgets;
    AttenuatorWidget* m_internalAttenuatorWidget = nullptr;

    // Cached so newly-added widgets get the current frequency at construction.
    // NaN until MainWindow first publishes a value.
    double m_currentFreqHz;

    // UI elements
    QVBoxLayout *m_listLayout;
    QComboBox *m_typeComboBox;
    QLCDNumber *m_totalLcd;
    QWidget *m_scrollContent = nullptr;
    QFrame *m_insertIndicator = nullptr;
};

#endif // ATTENUATIONMANAGER_H
