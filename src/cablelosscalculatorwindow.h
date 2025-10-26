#ifndef CABLELOSSCALCULATORWINDOW_H
#define CABLELOSSCALCULATORWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class CableLossCalculatorWindow; }
QT_END_NAMESPACE

class QtCoaxCableLossCalcManager;

class CableLossCalculatorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit CableLossCalculatorWindow(QWidget *parent = nullptr);
    ~CableLossCalculatorWindow();

private slots:
    void onSliderValueChanged(int value);
    void onStartFreqChanged(double value);
    void onEndFreqChanged(double value);
    void onCurrentFreqSpinBoxChanged(double value);

private:
    void setupFrequencyControls();

    Ui::CableLossCalculatorWindow *ui;
    QtCoaxCableLossCalcManager *m_manager;
};

#endif // CABLELOSSCALCULATORWINDOW_H
