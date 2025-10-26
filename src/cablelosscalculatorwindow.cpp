#include "cablelosscalculatorwindow.h"
#include "ui_cablelosscalculatorwindow.h"
#include "qtcoaxcablelosscalcmanager.h"

#include <QVBoxLayout>
#include <QDebug>

CableLossCalculatorWindow::CableLossCalculatorWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CableLossCalculatorWindow)
{
    ui->setupUi(this);

    m_manager = new QtCoaxCableLossCalcManager(this);
    m_manager->loadCablesFromJson(":/cables.json");

    m_manager->setIndividualLengthAllowed(true);
    m_manager->setAllowCableDupes(true);

    ui->cableManager_groupBox->setLayout(new QVBoxLayout());
    ui->cableManager_groupBox->layout()->addWidget(m_manager);

    setupFrequencyControls();

    m_manager->setGlobalLength(ui->length_doubleSpinBox->value());
}

CableLossCalculatorWindow::~CableLossCalculatorWindow()
{
    delete ui;
}

void CableLossCalculatorWindow::setupFrequencyControls()
{
    ui->startFreq_doubleSpinBox->setDecimals(3);
    ui->startFreq_doubleSpinBox->setSingleStep(0.001);
    ui->startFreq_doubleSpinBox->setRange(0.001, 10000.0);

    ui->endFreq_doubleSpinBox->setDecimals(3);
    ui->endFreq_doubleSpinBox->setSingleStep(0.001);
    ui->endFreq_doubleSpinBox->setRange(0.001, 10000.0);

    ui->currentFreq_doubleSpinBox->setDecimals(3);
    ui->currentFreq_doubleSpinBox->setSingleStep(0.001);

    ui->startFreq_doubleSpinBox->setValue(1.0);
    ui->endFreq_doubleSpinBox->setValue(6000.0);

    connect(ui->startFreq_doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CableLossCalculatorWindow::onStartFreqChanged);
    connect(ui->endFreq_doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CableLossCalculatorWindow::onEndFreqChanged);

    connect(ui->freq_horizontalSlider, &QSlider::valueChanged, this, &CableLossCalculatorWindow::onSliderValueChanged);
    connect(ui->currentFreq_doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CableLossCalculatorWindow::onCurrentFreqSpinBoxChanged);

    connect(ui->length_doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), m_manager, &QtCoaxCableLossCalcManager::setGlobalLength);

    onStartFreqChanged(ui->startFreq_doubleSpinBox->value());
    onEndFreqChanged(ui->endFreq_doubleSpinBox->value());
    onCurrentFreqSpinBoxChanged(ui->startFreq_doubleSpinBox->value());
    m_manager->setPlotRange(ui->startFreq_doubleSpinBox->value(), ui->endFreq_doubleSpinBox->value());
}

void CableLossCalculatorWindow::onSliderValueChanged(int value)
{
    double frequencyMHz = static_cast<double>(value) / 1000.0;
    m_manager->setFrequency(frequencyMHz);

    ui->currentFreq_doubleSpinBox->blockSignals(true);
    ui->currentFreq_doubleSpinBox->setValue(frequencyMHz);
    ui->currentFreq_doubleSpinBox->blockSignals(false);
}

void CableLossCalculatorWindow::onCurrentFreqSpinBoxChanged(double value)
{
    m_manager->setFrequency(value);

    int sliderValue = static_cast<int>(value * 1000);
    ui->freq_horizontalSlider->blockSignals(true);
    ui->freq_horizontalSlider->setValue(sliderValue);
    ui->freq_horizontalSlider->blockSignals(false);
}

void CableLossCalculatorWindow::onStartFreqChanged(double value)
{
    ui->endFreq_doubleSpinBox->setMinimum(value);
    ui->currentFreq_doubleSpinBox->setMinimum(value);
    ui->freq_horizontalSlider->setMinimum(static_cast<int>(value * 1000));
    m_manager->setPlotRange(value, ui->endFreq_doubleSpinBox->value());
}

void CableLossCalculatorWindow::onEndFreqChanged(double value)
{
    ui->startFreq_doubleSpinBox->setMaximum(value);
    ui->currentFreq_doubleSpinBox->setMaximum(value);
    ui->freq_horizontalSlider->setMaximum(static_cast<int>(value * 1000));
    m_manager->setPlotRange(ui->startFreq_doubleSpinBox->value(), value);
}
