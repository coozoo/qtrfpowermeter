#include "calibrationmanager.h"
#include "ui_calibrationmanager.h"
#include "tinysasourcecontroller.h"
#include "devicecalibrationviewerwidget.h"
#include "advancedcalibrationtablemodel.h"
#include <QStandardPaths>
#include <QSerialPortInfo>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QTableView>
#include <QHeaderView>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QToolButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QTimer>
#include <numeric>

// Settle window after the TinySa confirms a new frequency. Samples
// arriving inside this window are dropped so the meter has flushed
// stale readings from the previous frequency before we start averaging.
static constexpr int kSettleMs = 100;

CalibrationManager::CalibrationManager(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CalibrationManager)
{
    ui->setupUi(this);

    m_model = new CalibrationModel(this);
    ui->tableView->setModel(m_model);
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);

    // --- Connect model changes to the plot ---
    connect(m_model, &QAbstractItemModel::modelReset, this, &CalibrationManager::updatePlot);
    connect(m_model, &QAbstractItemModel::dataChanged, this, &CalibrationManager::updatePlot);


    // Connect UI signals to slots
    connect(ui->generateButton, &QToolButton::clicked, this, &CalibrationManager::ongenerateButton_clicked);
    connect(ui->pickAverageButton, &QPushButton::clicked, this, &CalibrationManager::onpickAverageButton_clicked);
    connect(ui->saveProfileButton, &QPushButton::clicked, this, &CalibrationManager::onsaveProfileButton_clicked);
    connect(ui->loadProfileButton, &QPushButton::clicked, this, &CalibrationManager::onloadProfileButton_clicked);
    connect(ui->deleteProfileButton, &QPushButton::clicked, this, &CalibrationManager::ondeleteProfileButton_clicked);
    connect(ui->profileComboBox, &QComboBox::currentTextChanged, this, &CalibrationManager::onprofileComboBox_currentIndexChanged);
    connect(ui->tableView, &QTableView::clicked, this, &CalibrationManager::ontable_clicked);

    // Setup unit comboboxes
    ui->startUnitComboBox->addItems({"MHz", "GHz"});
    ui->endUnitComboBox->addItems({"MHz", "GHz"});
    ui->stepUnitComboBox->addItems({"MHz", "GHz"});

    // --- Set initial precision for MHz ---
    ui->startFreqSpinBox->setDecimals(0);
    ui->endFreqSpinBox->setDecimals(0);
    ui->stepFreqSpinBox->setDecimals(0);
    ui->stepFreqSpinBox->setMinimum(5);

    // --- Connect each control to its own dedicated slot ---
    connect(ui->startFreqSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CalibrationManager::onStartFreqChanged);
    connect(ui->endFreqSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CalibrationManager::onEndFreqChanged);
    connect(ui->startUnitComboBox, &QComboBox::currentTextChanged, this, &CalibrationManager::onStartUnitChanged);
    connect(ui->endUnitComboBox, &QComboBox::currentTextChanged, this, &CalibrationManager::onEndUnitChanged);
    connect(ui->stepUnitComboBox, &QComboBox::currentTextChanged, this, &CalibrationManager::onStepUnitChanged);

    connect(ui->plotWidget, &QCustomPlot::mousePress, this, &CalibrationManager::onPlotMousePress);

    // --- Connect signals for saving settings on change ---
    connect(ui->profileComboBox, &QComboBox::currentTextChanged, this, [](const QString &text){
        QSettings settings;
        settings.setValue("CalibrationManager/profile", text);
    });
    connect(ui->referencePowerSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [](double value){
        QSettings settings;
        settings.setValue("CalibrationManager/referencePower", value);
    });
    connect(ui->sampleCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int value){
        QSettings settings;
        settings.setValue("CalibrationManager/sampleCount", value);
    });

    ui->calibrateSelected_progressBar->setVisible(false);
    ui->calibrateSelected_progressBar->setTextVisible(false);

    // Themed icons with resource fallback so the buttons render the same
    // way the platform's icon theme presents the rest of the chrome.
    ui->loadProfileButton->setIcon(QIcon::fromTheme(QStringLiteral("document-open"),
        QIcon(QStringLiteral(":/images/document-open.svg"))));
    ui->saveProfileButton->setIcon(QIcon::fromTheme(QStringLiteral("document-save"),
        QIcon(QStringLiteral(":/images/document-save.svg"))));
    ui->deleteProfileButton->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete"),
        QIcon(QStringLiteral(":/images/edit-delete.svg"))));
    // Sampling metaphor: the pipette icon reads as "pick the value here".
    ui->pickAverageButton->setIcon(QIcon::fromTheme(QStringLiteral("color-picker"),
        QIcon(QStringLiteral(":/images/color-picker.svg"))));
    ui->calibrateAllButton->setIcon(QIcon::fromTheme(QStringLiteral("color-picker"),
        QIcon(QStringLiteral(":/images/color-picker.svg"))));
    ui->tinySaRefreshButton->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh"),
        QIcon(QStringLiteral(":/images/view-refresh.svg"))));
    // Initial TinySa-connect icon reflects the disconnected state; the
    // onTinySaConnected/onTinySaDisconnected slots swap it on transitions.
    ui->tinySaConnectButton->setIcon(QIcon(QStringLiteral(":/images/network-offline.svg")));

    setupPlot();
    onStartFreqChanged(ui->startFreqSpinBox->value());
    onEndFreqChanged(ui->endFreqSpinBox->value());

    // --- 5e: Mode / Auto / TinySa wiring -------------------------------
    connect(ui->modeSimpleRadio,   &QRadioButton::toggled, this, &CalibrationManager::onModeChanged);
    connect(ui->modeAdvancedRadio, &QRadioButton::toggled, this, &CalibrationManager::onModeChanged);
    connect(ui->modeDisabledRadio, &QRadioButton::toggled, this, &CalibrationManager::onModeChanged);
    connect(ui->autoCalibrationCheckBox, &QCheckBox::toggled,
            this, &CalibrationManager::onAutoCheckBoxToggled);
    connect(ui->tinySaRefreshButton, &QPushButton::clicked,
            this, &CalibrationManager::onTinySaRefreshClicked);
    connect(ui->tinySaConnectButton, &QPushButton::clicked,
            this, &CalibrationManager::onTinySaConnectClicked);
    connect(ui->calibrateAllButton,  &QPushButton::clicked,
            this, &CalibrationManager::onCalibrateAllClicked);
    // Both calibrate buttons are checkable: their OR-ed checked state is
    // "calibration in flight", and that drives the table grey-out via the
    // refreshLock lambda below. Sweep-start/-end paths only toggle the
    // appropriate button; the text + tables follow automatically.
    ui->calibrateAllButton->setCheckable(true);
    ui->pickAverageButton->setCheckable(true);
    auto refreshLock = [this](bool) {
        const bool busy = ui->calibrateAllButton->isChecked()
                       || ui->pickAverageButton->isChecked();
        ui->tableView->setDisabled(busy);
        if (m_advancedTableView) m_advancedTableView->setDisabled(busy);
    };
    connect(ui->calibrateAllButton, &QPushButton::toggled, this, refreshLock);
    connect(ui->pickAverageButton,  &QPushButton::toggled, this, refreshLock);
    connect(ui->calibrateAllButton, &QPushButton::toggled, this, [this](bool busy) {
        ui->calibrateAllButton->setText(busy ? tr("Cancel calibration") : tr("Calibrate All"));
    });

    // Default UI shape: Simple, Auto off, TinySa subgroup hidden.
    applyModeUi(CalibrationModel::Mode::Simple);
    ui->autoCalibrationCheckBox->setChecked(false);
    ui->tinySaSourceGroupBox->setVisible(false);
    refreshTinySaPorts();

    // --- 5f: Advanced editor scaffolding -------------------------------
    // Adapter view over CalibrationModel's 2D advanced store. Lives next
    // to the Simple QTableView; mode flip toggles visibility.
    m_advancedAdapter = new AdvancedCalibrationTableModel(m_model, this);

    m_advancedTableView = new QTableView(this);
    m_advancedTableView->setModel(m_advancedAdapter);
    m_advancedTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_advancedTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_advancedTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_advancedTableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_advancedTableView->setVisible(false);

    m_advancedAxisRow = new QWidget(this);
    auto *axisLayout = new QHBoxLayout(m_advancedAxisRow);
    axisLayout->setContentsMargins(0, 0, 0, 0);
    axisLayout->addWidget(new QLabel(tr("Power axis (dBm):"), m_advancedAxisRow));
    axisLayout->addWidget(new QLabel(tr("Min"), m_advancedAxisRow));
    m_advancedAxisMinSpin = new QDoubleSpinBox(m_advancedAxisRow);
    m_advancedAxisMinSpin->setRange(-100.0, 100.0);
    m_advancedAxisMinSpin->setDecimals(1);
    m_advancedAxisMinSpin->setValue(-40.0);
    axisLayout->addWidget(m_advancedAxisMinSpin);
    axisLayout->addWidget(new QLabel(tr("Max"), m_advancedAxisRow));
    m_advancedAxisMaxSpin = new QDoubleSpinBox(m_advancedAxisRow);
    m_advancedAxisMaxSpin->setRange(-100.0, 100.0);
    m_advancedAxisMaxSpin->setDecimals(1);
    m_advancedAxisMaxSpin->setValue(10.0);
    axisLayout->addWidget(m_advancedAxisMaxSpin);
    axisLayout->addWidget(new QLabel(tr("Step"), m_advancedAxisRow));
    m_advancedAxisStepSpin = new QDoubleSpinBox(m_advancedAxisRow);
    // Floor at 2.5 dB to match the model's setAdvancedStepDb clamp.
    m_advancedAxisStepSpin->setRange(2.5, 50.0);
    m_advancedAxisStepSpin->setDecimals(1);
    m_advancedAxisStepSpin->setSingleStep(0.5);
    m_advancedAxisStepSpin->setValue(2.5);
    axisLayout->addWidget(m_advancedAxisStepSpin);
    m_advancedAxisApplyBtn = new QToolButton(m_advancedAxisRow);
    m_advancedAxisApplyBtn->setText(tr("Apply axis"));
    m_advancedAxisApplyBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    axisLayout->addWidget(m_advancedAxisApplyBtn);
    axisLayout->addStretch(1);
    m_advancedAxisRow->setVisible(false);

    if (auto *body = qobject_cast<QVBoxLayout*>(ui->calibrationBody->layout())) {
        // Insert axis row above the Simple tableView, advanced grid right
        // below it. Hidden widgets collapse so the layout still looks
        // identical in Simple mode.
        int simpleTableIndex = body->indexOf(ui->tableView);
        if (simpleTableIndex < 0) simpleTableIndex = 1;
        body->insertWidget(simpleTableIndex, m_advancedAxisRow);
        body->insertWidget(simpleTableIndex + 2, m_advancedTableView);
    }

    connect(m_advancedAxisApplyBtn, &QToolButton::clicked,
            this, &CalibrationManager::onAdvancedAxisApplyClicked);

    // Per-cell watchdog. Fires if no samples arrive within a generous
    // window after dispatch; treats the cell as unreachable and moves on.
    m_autoWatchdog = new QTimer(this);
    m_autoWatchdog->setSingleShot(true);
    connect(m_autoWatchdog, &QTimer::timeout, this, &CalibrationManager::onAutoCellTimeout);

    // Per-device persistence of the axis spinbox values. Same keying
    // convention as mode/auto: `CalibrationManager/<deviceId>/advancedAxis*`.
    auto persistAxis = [this]() {
        if (m_activeDeviceId.isEmpty()) return;
        QSettings s;
        s.setValue(settingsKeyFor("advancedAxisMin"),  m_advancedAxisMinSpin->value());
        s.setValue(settingsKeyFor("advancedAxisMax"),  m_advancedAxisMaxSpin->value());
        s.setValue(settingsKeyFor("advancedAxisStep"), m_advancedAxisStepSpin->value());
    };
    connect(m_advancedAxisMinSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, persistAxis);
    connect(m_advancedAxisMaxSpin,  QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, persistAxis);
    connect(m_advancedAxisStepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, persistAxis);

    // The Advanced chart is freq vs correction at the selected column.
    // Redraw on any underlying data change.
    connect(m_advancedAdapter, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex&, const QModelIndex&, const QList<int>&) {
                updateAdvancedColumnChart();
            });
    connect(m_advancedAdapter, &QAbstractItemModel::modelReset,
            this, [this]() { updateAdvancedColumnChart(); });

    // Selecting a cell in the grid: tune the DUT, set refPower to the
    // selected column's axis level (so Calibrate Selected -- both
    // manual and Auto -- targets that level), enable the button, and
    // redraw the chart.
    connect(m_advancedTableView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this](const QModelIndex &current, const QModelIndex&) {
                if (current.isValid()) {
                    const auto &rows = m_model->advancedRows();
                    const auto &axis = m_model->advancedPowerAxis();
                    if (current.row() >= 0 && current.row() < rows.size()) {
                        emit frequencySelected(rows[current.row()].frequencyMHz);
                    }
                    if (current.column() >= 0 && current.column() < axis.size()) {
                        mirrorRefSpinbox(axis[current.column()]);
                    }
                }
                refreshButtonEnabledState();
                updateAdvancedColumnChart();
            });

    // Set the initial enabled state once everything is wired up.
    // m_pmConnected = false, m_tinySaConnected = false, no selection yet,
    // so both calibrate buttons start disabled.
    refreshButtonEnabledState();
}

CalibrationManager::~CalibrationManager()
{
    // The TinySa controller is a child of `this`. When ~QWidget runs the
    // child-deletion loop, the controller stops its serial port, which
    // re-emits portClosed -> disconnected -> our onTinySaDisconnected
    // slot while `this` is already mid-destruction. Disconnect the
    // signal/slot wires first so those dispatches don't reach a
    // half-destroyed CalibrationManager (asserts in QtPrivate::assertObjectType).
    if (m_autoWatchdog) m_autoWatchdog->stop();
    if (m_tinySa) {
        disconnect(m_tinySa, nullptr, this, nullptr);
    }
    delete ui;
}

void CalibrationManager::loadSettings()
{
    QSettings settings;
    settings.beginGroup("CalibrationManager");

    ui->profileComboBox->blockSignals(true);
    ui->referencePowerSpinBox->blockSignals(true);
    ui->sampleCountSpinBox->blockSignals(true);

    ui->referencePowerSpinBox->setValue(settings.value("referencePower", -20.0).toDouble());
    ui->sampleCountSpinBox->setValue(settings.value("sampleCount", 5).toInt());
    QString lastProfile = settings.value("profile", "").toString();
    loadProfiles();

    int profileIndex = ui->profileComboBox->findText(lastProfile);
    if (profileIndex != -1) {
        ui->profileComboBox->setCurrentIndex(profileIndex);
    }

    ui->profileComboBox->blockSignals(false);
    ui->referencePowerSpinBox->blockSignals(false);
    ui->sampleCountSpinBox->blockSignals(false);

    if(ui->profileComboBox->currentIndex() != -1) {
        onloadProfileButton_clicked();
    }

    settings.endGroup();
}


void CalibrationManager::setupPlot()
{
    qDebug()<<Q_FUNC_INFO;

    QSizePolicy sp = ui->plotWidget->sizePolicy();
    sp.setHorizontalPolicy(QSizePolicy::Ignored);
    ui->plotWidget->setSizePolicy(sp);

    ui->plotWidget->addGraph();
    ui->plotWidget->graph(0)->setPen(QPen(Qt::blue));
    ui->plotWidget->addGraph();
    ui->plotWidget->graph(1)->setPen(QPen(Qt::red));
    ui->plotWidget->graph(1)->setLineStyle(QCPGraph::lsNone);
    ui->plotWidget->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));

    m_highlightGraph = ui->plotWidget->addGraph();
    m_highlightGraph->setPen(QPen(QColor(0, 255, 0, 150), 3));
    m_highlightGraph->setLineStyle(QCPGraph::lsNone);
    m_highlightGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCrossCircle, 12));
    m_highlightGraph->setVisible(false);

    // Advanced-mode curve: freq vs correction at the currently selected
    // column (power level). Shares plotWidget; hidden in Simple mode.
    m_advancedColumnGraph = ui->plotWidget->addGraph();
    m_advancedColumnGraph->setPen(QPen(Qt::darkGreen));
    m_advancedColumnGraph->setLineStyle(QCPGraph::lsLine);
    m_advancedColumnGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 5));
    m_advancedColumnGraph->setVisible(false);

    ui->plotWidget->xAxis->setLabel(m_model->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString());
    ui->plotWidget->yAxis->setLabel(m_model->headerData(1, Qt::Horizontal, Qt::DisplayRole).toString());
    //ui->plotWidget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
}

void CalibrationManager::updatePlot()
{
    qDebug()<<Q_FUNC_INFO;
    if (m_model->mode() != CalibrationModel::Mode::Simple) {
        // Advanced/Disabled use their own plot path (or no plot).
        return;
    }
    QVector<double> measuredFreqs, measuredCorrections;
    QVector<double> lineFreqs, lineCorrections;

    const auto &points = m_model->getPoints();
    if (points.isEmpty())
        {
            ui->plotWidget->graph(0)->setData({}, {});
            ui->plotWidget->graph(1)->setData({}, {});
            m_highlightGraph->setVisible(false);
            ui->plotWidget->replot();
            return;
        }
    else
        {
            for (const auto &point : points)
                {
                    if (point.isSet)
                        {
                            measuredFreqs.append(point.frequencyMHz);
                            measuredCorrections.append(point.correctionDb);
                        }
                }

            const int lineSteps = 200;
            double minFreq = points.first().frequencyMHz;
            double maxFreq = points.last().frequencyMHz;
            double step = (maxFreq - minFreq) / lineSteps;

            if (step > 0)
                {
                    for (int i = 0; i <= lineSteps; ++i)
                        {
                            double freq = minFreq + i * step;
                            lineFreqs.append(freq);
                            lineCorrections.append(m_model->getCorrection(freq));
                        }
                }
            else
                {
                    lineFreqs.append(minFreq);
                    lineCorrections.append(m_model->getCorrection(minFreq));
                }

            ui->plotWidget->graph(0)->setData(lineFreqs, lineCorrections);
            ui->plotWidget->graph(1)->setData(measuredFreqs, measuredCorrections);

            // Ensure highlight is updated if selection exists
            if (m_selectedIndex.isValid())
                {
                    highlightPoint(m_selectedIndex);
                }
            else
                {
                    m_highlightGraph->setVisible(false);
                }

            ui->plotWidget->rescaleAxes();
            QCPRange yRange = ui->plotWidget->yAxis->range();

            double padding = yRange.size() * 0.1;
            ui->plotWidget->yAxis->setRange(yRange.lower - padding, yRange.upper + padding);
            ui->plotWidget->replot();
        }
}

void CalibrationManager::highlightPoint(const QModelIndex &index)
{
    qDebug() << Q_FUNC_INFO;
    if (!index.isValid())
        {
            m_highlightGraph->setVisible(false);
            ui->plotWidget->replot();
            return;
        }
    bool freqOk, corrOk;
    double freq = m_model->data(m_model->index(index.row(), CalibrationModel::Frequency), Qt::EditRole).toDouble(&freqOk);
    double corr = m_model->data(m_model->index(index.row(), CalibrationModel::Correction), Qt::EditRole).toDouble(&corrOk);

    if (freqOk && corrOk)
        {
            m_highlightGraph->setData({freq}, {corr});
            m_highlightGraph->setVisible(true);
        }
    else
        {
            m_highlightGraph->setVisible(false);
        }
    ui->plotWidget->replot();
}

void CalibrationManager::onPlotMousePress(QMouseEvent *event)
{
    qDebug() << Q_FUNC_INFO;
    QCPGraph *graph = (m_model->mode() == CalibrationModel::Mode::Advanced)
                          ? m_advancedColumnGraph
                          : ui->plotWidget->graph(1);
    if (!graph || graph->dataCount() == 0) return;

    double minDist = std::numeric_limits<double>::max();
    int closestDataIndex = -1;

    for (int i = 0; i < graph->dataCount(); ++i)
        {
            QPointF dataPointPixel = graph->coordsToPixels(graph->dataMainKey(i), graph->dataMainValue(i));
            double dist = qSqrt(qPow(dataPointPixel.x() - event->pos().x(), 2) + qPow(dataPointPixel.y() - event->pos().y(), 2));

            if (dist < minDist)
                {
                    minDist = dist;
                    closestDataIndex = i;
                }
        }

    double threshold = 20; // Click within 20 pixels
    if (closestDataIndex == -1 || minDist >= threshold) return;

    const double targetFreq = graph->dataMainKey(closestDataIndex);
    if (m_model->mode() == CalibrationModel::Mode::Advanced) {
        // Find the matching row in advancedRows and select it in the
        // advanced grid. The selectionModel's currentChanged hook then
        // emits frequencySelected and redraws the chart's y-axis label.
        const auto &rows = m_model->advancedRows();
        for (int r = 0; r < rows.size(); ++r) {
            if (qFuzzyCompare(rows[r].frequencyMHz, targetFreq)) {
                const int col = qMax(0, m_advancedTableView->currentIndex().column());
                m_advancedTableView->setCurrentIndex(m_advancedAdapter->index(r, col));
                break;
            }
        }
        return;
    }

    for (int row = 0; row < m_model->rowCount(); ++row) {
        QModelIndex index = m_model->index(row, CalibrationModel::Frequency);
        double modelFreq = m_model->data(index, Qt::EditRole).toDouble();
        if (qFuzzyCompare(modelFreq, targetFreq)) {
            ui->tableView->selectRow(row);
            ontable_clicked(index);
            break;
        }
    }
}


void CalibrationManager::onStartFreqChanged(double value)
{
    qDebug()<<Q_FUNC_INFO;
    double startValMHz = value * (ui->startUnitComboBox->currentText() == "GHz" ? 1000.0 : 1.0);
    double newEndMin = (startValMHz + 1.0) / (ui->endUnitComboBox->currentText() == "GHz" ? 1000.0 : 1.0);

    ui->endFreqSpinBox->blockSignals(true);
    ui->endFreqSpinBox->setMinimum(newEndMin);
    ui->endFreqSpinBox->blockSignals(false);
}

void CalibrationManager::onEndFreqChanged(double value)
{
    qDebug()<<Q_FUNC_INFO;
    double endValMHz = value * (ui->endUnitComboBox->currentText() == "GHz" ? 1000.0 : 1.0);
    double newStartMax = (endValMHz - 1.0) / (ui->startUnitComboBox->currentText() == "GHz" ? 1000.0 : 1.0);

    ui->startFreqSpinBox->blockSignals(true);
    ui->startFreqSpinBox->setMaximum(newStartMax);
    ui->startFreqSpinBox->blockSignals(false);
}

void CalibrationManager::onStartUnitChanged(const QString &newUnit)
{
    qDebug()<<Q_FUNC_INFO;
    ui->startFreqSpinBox->blockSignals(true);
    double currentVal = ui->startFreqSpinBox->value();
    double multiplier = (newUnit == "GHz") ? 0.001 : 1000.0;
    double newCalculatedValue = currentVal * multiplier;
    ui->startFreqSpinBox->setDecimals((newUnit == "MHz") ? 0 : 3);
    double storedMaxLimit = ui->startFreqSpinBox->maximum();
    ui->startFreqSpinBox->setMaximum(100000.0);
    ui->startFreqSpinBox->setValue(newCalculatedValue);
    ui->startFreqSpinBox->setMaximum(storedMaxLimit * multiplier);
    ui->startFreqSpinBox->blockSignals(false);
}

void CalibrationManager::onEndUnitChanged(const QString &newUnit)
{
    qDebug()<<Q_FUNC_INFO;
    ui->endFreqSpinBox->blockSignals(true);
    double currentVal = ui->endFreqSpinBox->value();
    double multiplier = (newUnit == "GHz") ? 0.001 : 1000.0;
    double newCalculatedValue = currentVal * multiplier;
    ui->endFreqSpinBox->setDecimals((newUnit == "MHz") ? 0 : 3);
    double storedMinLimit = ui->endFreqSpinBox->minimum();
    ui->endFreqSpinBox->setMinimum(0);
    ui->endFreqSpinBox->setValue(newCalculatedValue);
    ui->endFreqSpinBox->setMinimum(storedMinLimit * multiplier);
    ui->endFreqSpinBox->blockSignals(false);
}


void CalibrationManager::onStepUnitChanged(const QString &newUnit)
{
    qDebug()<<Q_FUNC_INFO;
    ui->stepFreqSpinBox->blockSignals(true);
    double currentVal = ui->stepFreqSpinBox->value();
    double multiplier = (newUnit == "GHz") ? 0.001 : 1000.0;
    double newCalculatedValue = currentVal * multiplier;
    ui->stepFreqSpinBox->setDecimals((newUnit == "MHz") ? 0 : 3);
    double storedMinLimit = ui->stepFreqSpinBox->minimum();
    ui->stepFreqSpinBox->setMinimum(0);
    ui->stepFreqSpinBox->setValue(newCalculatedValue);
    ui->stepFreqSpinBox->setMinimum(storedMinLimit * multiplier);
    ui->stepFreqSpinBox->blockSignals(false);
}


void CalibrationManager::onDeviceConnectionStateChanged(bool connected)
{
    qDebug() << Q_FUNC_INFO << "Connected:" << connected;
    m_pmConnected = connected;
    if (!connected) {
        m_samplesToTake = 0;
        m_measurements.clear();
        ui->calibrateSelected_progressBar->setVisible(false);
        ui->pickAverageButton->setChecked(false);
        ui->calibrateAllButton->setChecked(false);
    }
    refreshButtonEnabledState();
}

void CalibrationManager::onNewMeasurement(double dbmValue)
{
    qDebug() << Q_FUNC_INFO ;
    // Auto path owns the measurement stream when the auto loop has a
    // cell in flight; finalises a cell as soon as enough samples land.
    if (m_autoCurrentRow >= 0) {
        onAutoSampleArrived(dbmValue);
        return;
    }
    // If we are not in measurement mode, just ignore the data.
    if (m_samplesToTake <= 0)
        {
            qDebug() << Q_FUNC_INFO << ": exiting because m_samplesToTake <= 0";
            return;
        }

    m_measurements.append(dbmValue);

    ui->calibrateSelected_progressBar->setValue(m_measurements.count());

    // Check if we have collected enough samples
    if (m_measurements.count() >= m_samplesToTake)
        {
            qDebug() << Q_FUNC_INFO << ": measurement complete.";
            double sum = std::accumulate(m_measurements.begin(), m_measurements.end(), 0.0);
            double averageDbm = sum / m_measurements.count();

            double referenceDbm = ui->referencePowerSpinBox->value();
            double correction = referenceDbm - averageDbm;

            if (m_model->mode() == CalibrationModel::Mode::Advanced
                && m_manualAdvancedRow >= 0 && m_manualAdvancedCol >= 0) {
                m_model->setAdvancedCell(m_manualAdvancedRow, m_manualAdvancedCol, correction);
                updateAdvancedColumnChart();
                m_manualAdvancedRow = -1;
                m_manualAdvancedCol = -1;
            } else if (m_selectedIndex.isValid()) {
                m_model->setData(m_model->index(m_selectedIndex.row(),
                                                CalibrationModel::Correction),
                                 correction, Qt::EditRole);
            }
            m_samplesToTake = 0;

            ui->calibrateSelected_progressBar->setVisible(false);
            ui->pickAverageButton->setChecked(false);
        }
}


void CalibrationManager::ongenerateButton_clicked()
{
    qDebug()<<Q_FUNC_INFO;
    double startFreq = ui->startFreqSpinBox->value();
    if (ui->startUnitComboBox->currentText() == "GHz") startFreq *= 1000.0;

    double endFreq = ui->endFreqSpinBox->value();
    if (ui->endUnitComboBox->currentText() == "GHz") endFreq *= 1000.0;

    double stepFreq = ui->stepFreqSpinBox->value();
    if (ui->stepUnitComboBox->currentText() == "GHz") stepFreq *= 1000.0;

    if (stepFreq <= 0)
        {
            QMessageBox::warning(this, tr("Invalid Step"), tr("Frequency step must be greater than zero."));
            return;
        }
    if (startFreq >= endFreq)
        {
            QMessageBox::warning(this, tr("Invalid Range"), tr("Start frequency must be less than end frequency."));
            return;
        }
    if (m_model->mode() == CalibrationModel::Mode::Advanced) {
        // Mirror Simple's row-generation: precise start, nice
        // step-aligned points in between, precise end. No
        // floating-point drift, no offset after the first row.
        m_model->generateAdvancedFrequencies(startFreq, endFreq, stepFreq);
    } else {
        m_model->generateFrequencies(startFreq, endFreq, stepFreq);
    }
}

void CalibrationManager::onAdvancedAxisApplyClicked()
{
    if (!m_advancedAxisMinSpin || !m_advancedAxisMaxSpin || !m_advancedAxisStepSpin) return;
    const double minDb  = m_advancedAxisMinSpin->value();
    const double maxDb  = m_advancedAxisMaxSpin->value();
    const double stepDb = m_advancedAxisStepSpin->value();
    if (minDb >= maxDb) {
        QMessageBox::warning(this, tr("Invalid power axis"),
                             tr("Min must be less than Max."));
        return;
    }
    m_model->generateAdvancedPowerAxis(minDb, maxDb, stepDb);
}

void CalibrationManager::ontable_clicked(const QModelIndex &index)
{
    qDebug()<<Q_FUNC_INFO;
    if (!index.isValid()) return;

    m_selectedIndex = index;
    double freq = m_model->data(m_model->index(index.row(), CalibrationModel::Frequency)).toDouble();
    refreshButtonEnabledState();
    highlightPoint(index);
    emit frequencySelected(freq);
}

void CalibrationManager::onpickAverageButton_clicked()
{
    qDebug()<<Q_FUNC_INFO;
    // Re-click guard. The button is checkable so Qt auto-toggles its
    // checked state on each click. If a calibration is already running,
    // ignore the click and force the "running" indicator back on.
    if (m_samplesToTake > 0 || m_autoCurrentRow >= 0) {
        ui->pickAverageButton->setChecked(true);
        return;
    }
    if (m_model->mode() == CalibrationModel::Mode::Advanced) {
        const QModelIndex curr = m_advancedTableView
                                     ? m_advancedTableView->currentIndex()
                                     : QModelIndex();
        if (!curr.isValid()) {
            QMessageBox::information(this, tr("No Selection"),
                                     tr("Please pick a cell in the Advanced grid first."));
            ui->pickAverageButton->setChecked(false);
            return;
        }
        if (ui->autoCalibrationCheckBox->isChecked() && !m_tinySaConnected) {
            QMessageBox::information(this, tr("TinySa not connected"),
                                     tr("Please connect the TinySa first."));
            ui->pickAverageButton->setChecked(false);
            return;
        }
        m_manualAdvancedRow = curr.row();
        m_manualAdvancedCol = curr.column();
        if (ui->autoCalibrationCheckBox->isChecked()) {
            startAutoCellForCurrentSelection();
            return;
        }
        // Manual: user drives the signal externally; we just average
        // the meter samples and store correction = refPower - avg in
        // the (row, col) cell on completion.
        m_measurements.clear();
        m_samplesToTake = ui->sampleCountSpinBox->value();
        ui->calibrateSelected_progressBar->setRange(0, m_samplesToTake);
        ui->calibrateSelected_progressBar->setValue(0);
        ui->calibrateSelected_progressBar->setVisible(true);
        return;
    }

    if (!m_selectedIndex.isValid())
        {
            qDebug() << Q_FUNC_INFO << "Aborting: No item selected in table.";
            QMessageBox::information(this, tr("No Selection"), tr("Please select a frequency in the table first."));
            ui->pickAverageButton->setChecked(false);
            return;
        }
    // Auto path: drive TinySa to (selectedFreq, RefPower) and let the
    // outputSet signal trigger sample collection. The legacy manual
    // path stays exactly as before for non-Auto runs.
    if (ui->autoCalibrationCheckBox->isChecked()) {
        if (!m_tinySaConnected) {
            QMessageBox::information(this, tr("TinySa not connected"),
                                     tr("Please connect the TinySa first."));
            ui->pickAverageButton->setChecked(false);
            return;
        }
        startAutoCellForCurrentSelection();
        return;
    }
    m_measurements.clear();
    m_samplesToTake = ui->sampleCountSpinBox->value();
    qDebug() << Q_FUNC_INFO << "Starting. m_samplesToTake set to" << m_samplesToTake;

    ui->calibrateSelected_progressBar->setRange(0, m_samplesToTake);
    ui->calibrateSelected_progressBar->setValue(0);
    ui->calibrateSelected_progressBar->setVisible(true);
}

double CalibrationManager::getCorrection(double frequencyMHz) const
{
    qDebug() << Q_FUNC_INFO << "for freq:" << frequencyMHz;
    // While an auto-fill cell is in flight, hold the apply path at 0
    // so the new measurement is computed against the raw meter reading.
    // MainWindow already emits raw dBm into newMeasurement(), but this
    // guards anything else that asks for correction during the window.
    if (m_autoCurrentRow >= 0) return 0.0;
    return m_model->getCorrection(frequencyMHz);
}

void CalibrationManager::setActiveProfile(const QString &name)
{
    qDebug()<<Q_FUNC_INFO;
    if (ui->profileComboBox->findText(name) != -1)
        {
            ui->profileComboBox->setCurrentText(name);
        }
}

QString CalibrationManager::getProfilesPath() const
{
    qDebug()<<Q_FUNC_INFO;
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(path);
    if (!dir.exists("calibration"))
        {
            dir.mkdir("calibration");
        }
    return path + "/calibration";
}

void CalibrationManager::loadProfiles()
{
    qDebug()<<Q_FUNC_INFO;
    ui->profileComboBox->clear();
    QDir dir(getProfilesPath());
    QStringList filters;
    filters<<"*.json";
    QFileInfoList list = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);

    for (const QFileInfo &fileInfo : list)
        {
            ui->profileComboBox->addItem(fileInfo.baseName());
        }
}

void CalibrationManager::onsaveProfileButton_clicked()
{
    qDebug()<<Q_FUNC_INFO;
    QString name = ui->profileComboBox->currentText();
    if (name.isEmpty())
        {
            QMessageBox::warning(this, tr("Invalid Name"), tr("Please enter a profile name."));
            return;
        }
    saveProfile(name);
    if (ui->profileComboBox->findText(name) == -1)
        {
            ui->profileComboBox->addItem(name);
        }
    ui->profileComboBox->setCurrentText(name);
    QMessageBox::information(this, tr("Success"), tr("Profile '%1' saved.").arg(name));
}

void CalibrationManager::onloadProfileButton_clicked()
{
    qDebug()<<Q_FUNC_INFO;
    QString name = ui->profileComboBox->currentText();
    if (name.isEmpty()) return;

    QFile file(getProfilesPath() + "/" + name + ".json");
    if (!file.open(QIODevice::ReadOnly))
        {
            qWarning() << "Could not load profile" << name << " - file does not exist or is not readable.";
            m_model->setPoints({});
            m_model->clearAdvanced();
            return;
        }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!m_model->loadFromJson(doc.object())) {
        qWarning() << "Profile" << name << "has unrecognised schema; ignoring";
        return;
    }

    // Reflect the loaded mode + axis settings in the UI.  ConceptRF lock,
    // if active, owns the mode display and overrides whatever was saved.
    if (!m_conceptLocked) {
        const CalibrationModel::Mode mode = m_model->mode();
        QSignalBlocker bs(ui->modeSimpleRadio);
        QSignalBlocker ba(ui->modeAdvancedRadio);
        QSignalBlocker bd(ui->modeDisabledRadio);
        ui->modeSimpleRadio->setChecked(mode == CalibrationModel::Mode::Simple);
        ui->modeAdvancedRadio->setChecked(mode == CalibrationModel::Mode::Advanced);
        ui->modeDisabledRadio->setChecked(mode == CalibrationModel::Mode::Disabled);
        applyModeUi(mode);
    }
    syncAdvancedAxisSpinsFromModel();
}

void CalibrationManager::ondeleteProfileButton_clicked()
{
    qDebug()<<Q_FUNC_INFO;
    QString name = ui->profileComboBox->currentText();
    if (name.isEmpty()) return;

    if (QMessageBox::question(this, tr("Confirm Delete"), tr("Are you sure you want to delete the profile '%1'?").arg(name)) == QMessageBox::Yes)
        {
            QFile file(getProfilesPath() + "/" + name + ".json");
            if (file.remove())
                {
                    QMessageBox::information(this, tr("Success"), tr("Profile '%1' deleted.").arg(name));
                    loadProfiles(); 
                    m_model->setPoints({});
                }
            else
                {
                    QMessageBox::critical(this, tr("Error"), tr("Could not delete profile '%1'.").arg(name));
                }
        }
}

void CalibrationManager::onprofileComboBox_currentIndexChanged(const QString &name)
{
    qDebug()<<Q_FUNC_INFO;
    if (!name.isEmpty())
        {
            emit currentProfileChanged(name);
        }
    int index = ui->profileComboBox->findText(ui->profileComboBox->currentText(), Qt::MatchFixedString);
    if (index != -1)
        {
            ui->loadProfileButton->setEnabled(true);
            ui->deleteProfileButton->setEnabled(true);
        }
    else
        {
            ui->loadProfileButton->setEnabled(false);
            ui->deleteProfileButton->setEnabled(false);
        }
}

void CalibrationManager::saveProfile(const QString &name)
{
    qDebug()<<Q_FUNC_INFO;
    QFile file(getProfilesPath() + "/" + name + ".json");
    if (!file.open(QIODevice::WriteOnly))
        {
            qWarning()<<"Could not open file for writing:"<<file.fileName();
            return;
        }

    // Use the model's full-schema JSON (mode + simple + advanced) so the
    // Advanced 2D table is preserved across save/load. The legacy "points"
    // array still loads via loadFromJson's migration path.
    QJsonObject rootObj = m_model->saveToJson();
    rootObj["name"] = name;
    file.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
}

void CalibrationManager::syncAdvancedAxisSpinsFromModel()
{
    if (!m_advancedAxisMinSpin || !m_advancedAxisMaxSpin || !m_advancedAxisStepSpin) return;
    const QVector<double> &axis = m_model->advancedPowerAxis();
    if (axis.isEmpty()) return;
    QSignalBlocker bMin(m_advancedAxisMinSpin);
    QSignalBlocker bMax(m_advancedAxisMaxSpin);
    QSignalBlocker bStep(m_advancedAxisStepSpin);
    m_advancedAxisMinSpin->setValue(axis.first());
    m_advancedAxisMaxSpin->setValue(axis.last());
    m_advancedAxisStepSpin->setValue(m_model->advancedStepDb());
}

// =============================================================================
//  5e: Mode + Auto + TinySa source
// =============================================================================

void CalibrationManager::setActiveDeviceId(const QString &deviceId)
{
    if (m_activeDeviceId == deviceId) return;
    m_activeDeviceId = deviceId;
    if (m_activeDeviceId.isEmpty()) return;
    // ConceptRF lock owns the UI; don't override its forced-Disabled state.
    // The persisted state will be re-applied when the lock is released.
    if (m_conceptLocked) return;
    loadPersistedModeAndAuto();
}

void CalibrationManager::loadPersistedModeAndAuto()
{
    if (m_activeDeviceId.isEmpty()) return;
    QSettings s;
    const QString modeStr = s.value(settingsKeyFor("mode"), QStringLiteral("simple")).toString();
    const bool    autoOn  = s.value(settingsKeyFor("auto"), false).toBool();
    const CalibrationModel::Mode mode = CalibrationModel::modeFromString(modeStr);

    {
        QSignalBlocker bs(ui->modeSimpleRadio);
        QSignalBlocker ba(ui->modeAdvancedRadio);
        QSignalBlocker bd(ui->modeDisabledRadio);
        ui->modeSimpleRadio->setChecked(mode == CalibrationModel::Mode::Simple);
        ui->modeAdvancedRadio->setChecked(mode == CalibrationModel::Mode::Advanced);
        ui->modeDisabledRadio->setChecked(mode == CalibrationModel::Mode::Disabled);
    }
    m_model->setMode(mode);
    applyModeUi(mode);

    QSignalBlocker bauto(ui->autoCalibrationCheckBox);
    ui->autoCalibrationCheckBox->setChecked(autoOn);
    ui->tinySaSourceGroupBox->setVisible(autoOn && mode != CalibrationModel::Mode::Disabled);

    // Restore the per-device Advanced power-axis spinbox state. Defaults
    // match the constructor seeds when no saved value exists.
    if (m_advancedAxisMinSpin && m_advancedAxisMaxSpin && m_advancedAxisStepSpin) {
        const double axisMin  = s.value(settingsKeyFor("advancedAxisMin"),  -40.0).toDouble();
        const double axisMax  = s.value(settingsKeyFor("advancedAxisMax"),   10.0).toDouble();
        const double axisStep = s.value(settingsKeyFor("advancedAxisStep"),   2.5).toDouble();
        QSignalBlocker bMin(m_advancedAxisMinSpin);
        QSignalBlocker bMax(m_advancedAxisMaxSpin);
        QSignalBlocker bStep(m_advancedAxisStepSpin);
        m_advancedAxisMinSpin->setValue(axisMin);
        m_advancedAxisMaxSpin->setValue(axisMax);
        m_advancedAxisStepSpin->setValue(axisStep);
    }
}

void CalibrationManager::setActiveConceptRfDevice(const ConceptRfRpmDevice *device)
{
    const bool wasLocked = m_conceptLocked;
    m_conceptLocked = (device != nullptr);

    if (m_conceptLocked) {
        if (!m_factoryView) {
            m_factoryView = new DeviceCalibrationViewerWidget(this);
            // Append below calibrationBody; applyModeUi hides the body
            // when locked so the factory view fills the panel.
            qobject_cast<QVBoxLayout*>(layout())->addWidget(m_factoryView, 1);
        }
        m_factoryView->setDevice(device);
        m_factoryView->setVisible(true);

        // Force radios to Disabled and lock them.
        {
            QSignalBlocker bs(ui->modeSimpleRadio);
            QSignalBlocker ba(ui->modeAdvancedRadio);
            QSignalBlocker bd(ui->modeDisabledRadio);
            ui->modeSimpleRadio->setChecked(false);
            ui->modeAdvancedRadio->setChecked(false);
            ui->modeDisabledRadio->setChecked(true);
        }
        ui->modeSimpleRadio->setEnabled(false);
        ui->modeAdvancedRadio->setEnabled(false);
        ui->modeDisabledRadio->setEnabled(false);

        m_model->setMode(CalibrationModel::Mode::Disabled);
        ui->disabledBanner->setText(
            tr("ConceptRF applies factory calibration internally; user calibration off."));
        applyModeUi(CalibrationModel::Mode::Disabled);
        return;
    }

    if (wasLocked) {
        if (m_factoryView) {
            m_factoryView->setDevice(nullptr);
            m_factoryView->setVisible(false);
        }
        ui->modeSimpleRadio->setEnabled(true);
        ui->modeAdvancedRadio->setEnabled(true);
        ui->modeDisabledRadio->setEnabled(true);
        ui->disabledBanner->setText(
            tr("Calibration is disabled for this device."));
        loadPersistedModeAndAuto();
    }
}

QString CalibrationManager::settingsKeyFor(const QString &leaf) const
{
    const QString id = m_activeDeviceId.isEmpty()
                           ? QStringLiteral("default")
                           : m_activeDeviceId;
    return QString("CalibrationManager/%1/%2").arg(id, leaf);
}

double CalibrationManager::getCorrection(double frequencyMHz, double measuredDbm) const
{
    if (m_autoCurrentRow >= 0) return 0.0;
    return m_model->getCorrection(frequencyMHz, measuredDbm);
}

// -----------------------------------------------------------------------------
//  Mode handling
// -----------------------------------------------------------------------------

void CalibrationManager::onModeChanged()
{
    CalibrationModel::Mode mode = CalibrationModel::Mode::Simple;
    if (ui->modeAdvancedRadio->isChecked()) mode = CalibrationModel::Mode::Advanced;
    else if (ui->modeDisabledRadio->isChecked()) mode = CalibrationModel::Mode::Disabled;
    m_model->setMode(mode);
    applyModeUi(mode);
    persistMode(mode);
    // Selection semantics differ per mode (m_selectedIndex vs the
    // advanced grid's currentIndex), so the calibrate buttons need a
    // re-evaluation after a mode flip.
    refreshButtonEnabledState();
}

void CalibrationManager::applyModeUi(CalibrationModel::Mode mode)
{
    const bool disabled = (mode == CalibrationModel::Mode::Disabled);
    const bool advanced = (mode == CalibrationModel::Mode::Advanced);
    if (m_conceptLocked) {
        // Factory table view replaces the body entirely.
        ui->calibrationBody->setVisible(false);
        ui->disabledBanner->setVisible(true);
    } else {
        ui->calibrationBody->setVisible(true);
        ui->calibrationBody->setEnabled(!disabled);
        ui->disabledBanner->setVisible(disabled);
    }
    // Simple table is only shown in Simple mode; Advanced grid + axis row
    // only in Advanced mode. Disabled hides both.
    ui->tableView->setVisible(!disabled && !advanced);
    if (m_advancedTableView) m_advancedTableView->setVisible(!disabled && advanced);
    if (m_advancedAxisRow)   m_advancedAxisRow->setVisible(!disabled && advanced);

    // Calibration controls + TinySa source group are shared between
    // Simple and Advanced: visible whenever calibration is active.
    ui->calibrationGroupBox->setVisible(!disabled);

    // Toggle which plot curves are visible. In Advanced mode the Simple
    // curves go away in favour of the freq-vs-correction-at-column
    // curve. Both modes use freq on X / correction on Y, so axis labels
    // stay the same.
    if (ui->plotWidget->graphCount() > 0) {
        if (auto *g0 = ui->plotWidget->graph(0)) g0->setVisible(!advanced);
        if (auto *g1 = ui->plotWidget->graph(1)) g1->setVisible(!advanced);
    }
    if (m_highlightGraph)       m_highlightGraph->setVisible(false);
    if (m_advancedColumnGraph)  m_advancedColumnGraph->setVisible(advanced);
    ui->plotWidget->xAxis->setLabel(
        m_model->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString());
    ui->plotWidget->yAxis->setLabel(
        m_model->headerData(1, Qt::Horizontal, Qt::DisplayRole).toString());
    ui->plotWidget->replot();

    if (advanced) {
        // First entry into Advanced: seed the axis if the model has none
        // so the grid has columns to display.
        if (m_model->advancedPowerAxis().isEmpty()) {
            m_model->generateAdvancedPowerAxis(
                m_advancedAxisMinSpin->value(),
                m_advancedAxisMaxSpin->value(),
                m_advancedAxisStepSpin->value());
        }
        updateAdvancedColumnChart();
    }

    if (disabled) {
        // Generator side has nothing to do; collapse Auto too.
        QSignalBlocker b(ui->autoCalibrationCheckBox);
        ui->autoCalibrationCheckBox->setChecked(false);
        ui->tinySaSourceGroupBox->setVisible(false);
    }
}

void CalibrationManager::persistMode(CalibrationModel::Mode mode)
{
    if (m_activeDeviceId.isEmpty()) return;
    QSettings s;
    s.setValue(settingsKeyFor("mode"), CalibrationModel::modeToString(mode));
}

// -----------------------------------------------------------------------------
//  Auto checkbox & TinySa subgroup visibility
// -----------------------------------------------------------------------------

void CalibrationManager::onAutoCheckBoxToggled(bool checked)
{
    ui->tinySaSourceGroupBox->setVisible(checked
                                         && m_model->mode() != CalibrationModel::Mode::Disabled);
    persistAuto(checked);
    refreshButtonEnabledState();
}

void CalibrationManager::persistAuto(bool autoOn)
{
    if (m_activeDeviceId.isEmpty()) return;
    QSettings s;
    s.setValue(settingsKeyFor("auto"), autoOn);
}

// -----------------------------------------------------------------------------
//  TinySa port pick & lifecycle
// -----------------------------------------------------------------------------

void CalibrationManager::refreshTinySaPorts()
{
    ui->tinySaPortComboBox->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        // Filter by description containing "tinysa" (case-insensitive).
        // Matches "tinySA", "tinySA4", etc. The STM VID is shared with
        // RF-PM V5 (a generic STM32 virtual COM port) so a VID filter
        // would pull that in too.
        if (info.description().contains(QStringLiteral("tinysa"), Qt::CaseInsensitive)) {
            ui->tinySaPortComboBox->addItem(
                info.portName() + " (" + info.description() + ")",
                info.portName());
        }
    }
}

void CalibrationManager::onTinySaRefreshClicked()
{
    refreshTinySaPorts();
}

void CalibrationManager::onTinySaConnectClicked()
{
    if (!m_tinySa) {
        m_tinySa = new TinySaSourceController(this);
        connect(m_tinySa, &TinySaSourceController::connected,
                this, &CalibrationManager::onTinySaConnected);
        connect(m_tinySa, &TinySaSourceController::disconnected,
                this, &CalibrationManager::onTinySaDisconnected);
        connect(m_tinySa, &TinySaSourceController::deviceError,
                this, &CalibrationManager::onTinySaError);
        connect(m_tinySa, &TinySaSourceController::outputSet,
                this, &CalibrationManager::onTinySaOutputSet);
        connect(m_tinySa, &TinySaSourceController::outputClamped,
                this, &CalibrationManager::onTinySaOutputClamped);
    }
    if (m_tinySaConnected) {
        m_tinySa->disconnectFromPort();
        return;
    }
    const QString port = ui->tinySaPortComboBox->currentData().toString();
    if (port.isEmpty()) {
        ui->tinySaStatusLabel->setText(tr("Pick a port first."));
        return;
    }
    ui->tinySaStatusLabel->setText(tr("Connecting to %1...").arg(port));
    m_tinySa->connectToPort(port);
}

void CalibrationManager::onTinySaConnected()
{
    m_tinySaConnected = true;
    ui->tinySaConnectButton->setText(tr("Disconnect"));
    // Icon reflects the *current* state, not the action: wired = "you are
    // connected", offline = "you are disconnected".
    ui->tinySaConnectButton->setIcon(QIcon(QStringLiteral(":/images/network-wired.svg")));
    ui->tinySaStatusLabel->setText(
        tr("connected: %1 -- connect TinySa RF OUT to the power meter input via coax")
            .arg(m_tinySa->portName()));
    refreshButtonEnabledState();
}

void CalibrationManager::onTinySaDisconnected()
{
    m_tinySaConnected = false;
    ui->tinySaConnectButton->setText(tr("Connect"));
    // Mirror the icon swap in onTinySaConnected: revert to the "offline"
    // state-indicator icon when the port closes.
    ui->tinySaConnectButton->setIcon(QIcon(QStringLiteral(":/images/network-offline.svg")));
    ui->tinySaStatusLabel->setText(tr("not connected"));
    refreshButtonEnabledState();
    // If a Calibrate All loop is in flight, abort cleanly.
    const bool wasAuto = (m_autoCurrentRow >= 0) || m_autoSequence;
    m_autoQueue.clear();
    m_autoCurrentRow = -1;
    m_autoCurrentCol = -1;
    m_autoSequence = false;
    if (m_autoWatchdog) m_autoWatchdog->stop();
    if (wasAuto) mirrorRefSpinbox(m_autoAskedRefDbm);
    ui->calibrateAllButton->setChecked(false);
    ui->pickAverageButton->setChecked(false);
}

void CalibrationManager::onTinySaError(const QString &msg)
{
    ui->tinySaStatusLabel->setText(tr("error: %1").arg(msg));
}

void CalibrationManager::onTinySaOutputSet(double freqHz, double levelDbm)
{
    // Generator confirmed via sweep readback. Do NOT start counting
    // samples yet: the power meter still has buffered samples from the
    // previous frequency (at 10 Hz a ~1 s buffer = ~10 stale readings),
    // and the RF signal needs time to settle through the meter and its
    // averaging. Drop incoming samples for the settle window, then arm
    // the collection loop.
    Q_UNUSED(freqHz);
    Q_UNUSED(levelDbm);
    if (m_autoCurrentRow < 0) return;
    const int rowForThisCall = m_autoCurrentRow;
    m_measurements.clear();
    m_samplesToTake = 0;
    ui->calibrateSelected_progressBar->setVisible(false);

    // Keep the existing "setting X MHz / Y dBm" status visible -- the
    // settle window is short and the freq/level is what the user cares
    // about. No status overwrite here.
    QTimer::singleShot(kSettleMs, this, [this, rowForThisCall]() {
        // Cell may have been aborted (disconnect, timeout, mode change)
        // while we were settling.  Bail if state has moved on.
        if (m_autoCurrentRow != rowForThisCall) return;
        m_measurements.clear();
        m_samplesToTake = ui->sampleCountSpinBox->value();
        ui->calibrateSelected_progressBar->setRange(0, m_samplesToTake);
        ui->calibrateSelected_progressBar->setValue(0);
        ui->calibrateSelected_progressBar->setVisible(true);
    });
}

void CalibrationManager::onAutoCellTimeout()
{
    if (m_autoCurrentRow < 0) return;
    qDebug() << Q_FUNC_INFO << "row" << m_autoCurrentRow << "col" << m_autoCurrentCol << "timed out";
    const int row = m_autoCurrentRow;
    const int col = m_autoCurrentCol;
    m_samplesToTake = 0;
    m_measurements.clear();
    ui->calibrateSelected_progressBar->setVisible(false);
    if (col >= 0) {
        ui->tinySaStatusLabel->setText(
            tr("cell %1/%2: no signal (skipped)").arg(row).arg(col));
    } else {
        ui->tinySaStatusLabel->setText(
            tr("row %1: no signal (skipped)").arg(row));
    }
    m_autoCurrentRow = -1;
    m_autoCurrentCol = -1;
    if (m_autoSequence) {
        dispatchNextAutoCell();
    } else {
        mirrorRefSpinbox(m_autoAskedRefDbm);
        ui->pickAverageButton->setChecked(false);
    }
}

void CalibrationManager::onTinySaOutputClamped(double freqHz, double askedDbm, double predictedDbm)
{
    // Predicted clamp: emit a status hint but keep going. Auto-fill
    // skips truly-unreachable cells via isCellAchievable() before
    // sending anything; this branch fires when the firmware will
    // round the asked value into the envelope.
    ui->tinySaStatusLabel->setText(
        tr("clamp at %1 Hz: asked %2 dBm -> %3 dBm")
            .arg(freqHz, 0, 'f', 0)
            .arg(askedDbm, 0, 'f', 2).arg(predictedDbm, 0, 'f', 2));
}

// -----------------------------------------------------------------------------
//  Calibrate All (auto sequence)
// -----------------------------------------------------------------------------

void CalibrationManager::onCalibrateAllClicked()
{
    // If a sweep is already running this button acts as Cancel. Guards
    // against the second-trigger problem where re-clicking would
    // re-queue every cell and step on the in-flight one.
    if (m_autoSequence) {
        cancelAutoSweep();
        return;
    }
    if (!m_tinySaConnected) {
        QMessageBox::information(this, tr("TinySa not connected"),
                                 tr("Please connect the TinySa first."));
        ui->calibrateAllButton->setChecked(false);
        return;
    }
    m_autoQueue.clear();
    m_autoAskedRefDbm = ui->referencePowerSpinBox->value();

    if (m_model->mode() == CalibrationModel::Mode::Advanced) {
        m_autoMode = CalibrationModel::Mode::Advanced;
        enqueueAdvancedSweep();
        if (m_autoQueue.isEmpty()) {
            ui->tinySaStatusLabel->setText(tr("nothing to calibrate"));
            ui->calibrateAllButton->setChecked(false);
            return;
        }
        m_autoSequence = true;
        ui->calibrateAllButton->setChecked(true);
        dispatchNextAutoCell();
        return;
    }

    m_autoMode = CalibrationModel::Mode::Simple;
    const auto &points = m_model->getPoints();
    // Queue every cell. We no longer drop cells whose asked refPower is
    // outside the freq envelope; dispatchNextAutoCell picks a reachable
    // level inside the envelope (max - 5 dB) and that level is the one
    // fed into the correction formula. So every row gets a chance and
    // the ref always matches what the generator actually outputs.
    for (int i = 0; i < points.size(); ++i) {
        m_autoQueue.enqueue({i, -1});
    }
    if (m_autoQueue.isEmpty()) {
        ui->tinySaStatusLabel->setText(tr("nothing to calibrate"));
        ui->calibrateAllButton->setChecked(false);
        return;
    }
    m_autoSequence = true;
    ui->calibrateAllButton->setChecked(true);
    dispatchNextAutoCell();
}

void CalibrationManager::cancelAutoSweep()
{
    m_autoQueue.clear();
    m_autoSequence = false;
    m_autoCurrentRow = -1;
    m_autoCurrentCol = -1;
    if (m_autoWatchdog) m_autoWatchdog->stop();
    m_samplesToTake = 0;
    m_measurements.clear();
    ui->calibrateSelected_progressBar->setVisible(false);
    mirrorRefSpinbox(m_autoAskedRefDbm);
    // Turn the TinySa output off so the user isn't left with a live
    // signal at the last clamped freq/level after cancelling.
    if (m_tinySa && m_tinySaConnected) m_tinySa->pauseOutput();
    ui->calibrateAllButton->setChecked(false);
    ui->tinySaStatusLabel->setText(tr("calibration cancelled"));
}

void CalibrationManager::enqueueAdvancedSweep()
{
    m_autoQueue.clear();
    const auto &rows = m_model->advancedRows();
    const auto &axis = m_model->advancedPowerAxis();
    for (int r = 0; r < rows.size(); ++r) {
        const double freqHz = rows[r].frequencyMHz * 1e6;
        for (int c = 0; c < axis.size(); ++c) {
            const double askedDbm = axis[c];
            // Skip cells outside the freq envelope. Apply path
            // interpolates across the holes.
            if (m_tinySa && !m_tinySa->isCellAchievable(freqHz, askedDbm)) continue;
            m_autoQueue.enqueue({r, c});
        }
    }
}

double CalibrationManager::dispatchLevelFor(double freqHz, double askedDbm) const
{
    if (!m_tinySa) return askedDbm;
    const auto env = m_tinySa->envelopeAt(freqHz);
    // Step 5 dB inside the band so we sit comfortably away from the
    // firmware clamp, which can still drift.
    if (askedDbm > env.maxDbm) return env.maxDbm - 5.0;
    if (askedDbm < env.minDbm) return env.minDbm + 5.0;
    return askedDbm;
}

void CalibrationManager::mirrorRefSpinbox(double dbm)
{
    QSignalBlocker b(ui->referencePowerSpinBox);
    ui->referencePowerSpinBox->setValue(dbm);
}

void CalibrationManager::refreshButtonEnabledState()
{
    const bool autoOn = ui->autoCalibrationCheckBox->isChecked();
    const bool hasSelection =
        (m_model->mode() == CalibrationModel::Mode::Advanced)
            ? (m_advancedTableView && m_advancedTableView->currentIndex().isValid())
            : m_selectedIndex.isValid();
    // Calibrate Selected needs the power meter (for samples) and a target
    // row/cell. Auto mode also needs the TinySa source.
    ui->pickAverageButton->setEnabled(
        m_pmConnected && hasSelection && (!autoOn || m_tinySaConnected));
    // Calibrate All always uses the auto sweep path: needs PM + TinySa,
    // plus the Auto checkbox (legacy gate kept for parity).
    ui->calibrateAllButton->setEnabled(
        m_pmConnected && m_tinySaConnected && autoOn);
}

void CalibrationManager::scrollToCurrentAutoCell()
{
    if (m_autoCurrentRow < 0) return;
    if (m_autoMode == CalibrationModel::Mode::Advanced
        && m_advancedTableView && m_advancedAdapter) {
        const QModelIndex idx = m_advancedAdapter->index(
            m_autoCurrentRow, qMax(0, m_autoCurrentCol));
        m_advancedTableView->setCurrentIndex(idx);
        m_advancedTableView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
    } else {
        ui->tableView->selectRow(m_autoCurrentRow);
        ui->tableView->scrollTo(m_model->index(m_autoCurrentRow, 0),
                                QAbstractItemView::PositionAtCenter);
    }
}

void CalibrationManager::dispatchNextAutoCell()
{
    if (m_autoQueue.isEmpty()) {
        m_autoSequence = false;
        m_autoCurrentRow = -1;
        m_autoCurrentCol = -1;
        if (m_autoWatchdog) m_autoWatchdog->stop();
        // Restore the user-asked refPower so the next manual run starts
        // from the value they typed, not from whatever the last cell
        // clamped to.
        mirrorRefSpinbox(m_autoAskedRefDbm);
        ui->calibrateAllButton->setChecked(false);
        ui->tinySaStatusLabel->setText(tr("calibration sweep done"));
        return;
    }
    const auto cell = m_autoQueue.dequeue();
    m_autoCurrentRow = cell.first;
    m_autoCurrentCol = cell.second; // -1 for Simple, >=0 for Advanced

    double freqMHz = 0.0, asked = m_autoAskedRefDbm;
    if (m_autoMode == CalibrationModel::Mode::Advanced) {
        const auto &rows = m_model->advancedRows();
        const auto &axis = m_model->advancedPowerAxis();
        if (m_autoCurrentRow < 0 || m_autoCurrentRow >= rows.size()
            || m_autoCurrentCol < 0 || m_autoCurrentCol >= axis.size()) {
            // Stale index; skip and continue.
            dispatchNextAutoCell();
            return;
        }
        freqMHz = rows[m_autoCurrentRow].frequencyMHz;
        // Each cell's "ref" is its column axis value; we don't clamp
        // because enqueueAdvancedSweep already filtered out unreachable
        // cells.
        asked = axis[m_autoCurrentCol];
    } else {
        freqMHz = m_model->getPoints()[m_autoCurrentRow].frequencyMHz;
    }
    const double freqHz = freqMHz * 1e6;

    // Scroll the cell being calibrated into view so the user can watch
    // sweep progress; the tables are already greyed out via the
    // calibrateAllButton.toggled connection set up in the ctor.
    scrollToCurrentAutoCell();

    // Tune the DUT to this row's freq -- same signal Calibrate Selected
    // emits when the user clicks a row in the table.
    emit frequencySelected(freqMHz);

    // For Simple, clamp asked into the envelope. For Advanced, asked is
    // already known reachable so dispatchLevelFor is a no-op.
    m_autoCellRefDbm = dispatchLevelFor(freqHz, asked);
    mirrorRefSpinbox(m_autoCellRefDbm);

    // Clear the cell before measuring, so the apply path can't fold the
    // previous correction into the new measurement.
    if (m_autoMode == CalibrationModel::Mode::Advanced) {
        m_model->clearAdvancedCell(m_autoCurrentRow, m_autoCurrentCol);
    } else {
        m_model->setData(m_model->index(m_autoCurrentRow, CalibrationModel::Correction),
                         0.0, Qt::EditRole);
    }

    const QString suffix = (m_autoMode == CalibrationModel::Mode::Advanced)
            ? tr(" (cell %1/%2)").arg(m_autoCurrentRow).arg(m_autoCurrentCol)
            : QString();
    if (qFuzzyCompare(m_autoCellRefDbm, asked)) {
        ui->tinySaStatusLabel->setText(
            tr("setting %1 MHz / %2 dBm%3")
                .arg(freqMHz, 0, 'f', 3).arg(m_autoCellRefDbm).arg(suffix));
    } else {
        ui->tinySaStatusLabel->setText(
            tr("setting %1 MHz / %2 dBm (asked %3, clamped to band)%4")
                .arg(freqMHz, 0, 'f', 3).arg(m_autoCellRefDbm, 0, 'f', 1)
                .arg(asked, 0, 'f', 1).arg(suffix));
    }

    // Arm watchdog: settle delay plus a generous window for samples to
    // arrive at the meter's sampling rate.
    if (m_autoWatchdog) {
        m_autoWatchdog->start(kSettleMs + 8000);
    }
    m_tinySa->setOutput(freqHz, m_autoCellRefDbm);
}

void CalibrationManager::startAutoCellForCurrentSelection()
{
    if (!m_tinySaConnected) return;
    m_autoSequence = false;
    m_autoAskedRefDbm = ui->referencePowerSpinBox->value();

    if (m_model->mode() == CalibrationModel::Mode::Advanced) {
        const QModelIndex curr = m_advancedTableView->currentIndex();
        if (!curr.isValid()) return;
        m_autoCurrentRow = curr.row();
        m_autoCurrentCol = curr.column();
        m_autoMode = CalibrationModel::Mode::Advanced;
        // Build a one-cell queue and reuse dispatchNextAutoCell so the
        // setOutput, settle, watchdog, mirror, clear, and sample-arming
        // path is shared with the Calibrate All sweep.
        m_autoQueue.clear();
        m_autoCurrentRow = -1;
        m_autoCurrentCol = -1;
        m_autoQueue.enqueue({curr.row(), curr.column()});
        dispatchNextAutoCell();
        return;
    }

    if (!m_selectedIndex.isValid()) return;
    m_autoCurrentRow = m_selectedIndex.row();
    m_autoCurrentCol = -1;
    m_autoMode = CalibrationModel::Mode::Simple;
    scrollToCurrentAutoCell();
    const double freqMHz = m_model->getPoints()[m_autoCurrentRow].frequencyMHz;
    const double freqHz  = freqMHz * 1e6;
    // Tune the DUT to the row's freq (same signal the table click sends
    // for non-Auto Calibrate Selected). Belt-and-braces: the row may
    // already be selected, but on entry from Calibrate Selected with
    // Auto on, m_selectedIndex is what the user clicked.
    emit frequencySelected(freqMHz);
    m_autoCellRefDbm = dispatchLevelFor(freqHz, m_autoAskedRefDbm);
    mirrorRefSpinbox(m_autoCellRefDbm);
    // Same clear-before-measure as the sweep path.
    m_model->setData(m_model->index(m_autoCurrentRow, CalibrationModel::Correction),
                     0.0, Qt::EditRole);
    const double asked = m_autoAskedRefDbm;
    if (qFuzzyCompare(m_autoCellRefDbm, asked)) {
        ui->tinySaStatusLabel->setText(
            tr("setting %1 MHz / %2 dBm").arg(freqMHz, 0, 'f', 3).arg(m_autoCellRefDbm));
    } else {
        ui->tinySaStatusLabel->setText(
            tr("setting %1 MHz / %2 dBm (asked %3, clamped to band)")
                .arg(freqMHz, 0, 'f', 3).arg(m_autoCellRefDbm, 0, 'f', 1).arg(asked, 0, 'f', 1));
    }
    if (m_autoWatchdog) {
        m_autoWatchdog->start(kSettleMs + 8000);
    }
    m_tinySa->setOutput(freqHz, m_autoCellRefDbm);
}

void CalibrationManager::onAutoSampleArrived(double dbmValue)
{
    // Called by onNewMeasurement() when the auto loop owns the
    // measurement stream; finalises a cell once we have enough samples.
    if (m_autoCurrentRow < 0 || m_samplesToTake <= 0) return;
    m_measurements.append(dbmValue);
    ui->calibrateSelected_progressBar->setValue(m_measurements.count());
    if (m_measurements.count() < m_samplesToTake) return;

    const double sum = std::accumulate(m_measurements.begin(), m_measurements.end(), 0.0);
    const double avgDbm = sum / m_measurements.count();
    // Use the level we actually asked the TinySa for (which may differ
    // from the spinbox value when the asked level was outside the freq's
    // envelope and we clamped it). The generator outputs this and the
    // meter measures it; the correction lines them up.
    const double refDbm = m_autoCellRefDbm;
    const double correction = refDbm - avgDbm;

    if (m_autoMode == CalibrationModel::Mode::Advanced && m_autoCurrentCol >= 0) {
        m_model->setAdvancedCell(m_autoCurrentRow, m_autoCurrentCol, correction);
        updateAdvancedColumnChart();
    } else {
        m_model->setData(m_model->index(m_autoCurrentRow, CalibrationModel::Correction),
                         correction, Qt::EditRole);
    }
    m_samplesToTake = 0;
    ui->calibrateSelected_progressBar->setVisible(false);
    if (m_autoWatchdog) m_autoWatchdog->stop();

    if (m_autoSequence) {
        dispatchNextAutoCell();
    } else {
        m_autoCurrentRow = -1;
        m_autoCurrentCol = -1;
        // Single-cell run done: put the user-asked refPower back in the
        // spinbox.
        mirrorRefSpinbox(m_autoAskedRefDbm);
        ui->pickAverageButton->setChecked(false);
    }
}

void CalibrationManager::updateAdvancedColumnChart()
{
    if (!m_advancedColumnGraph) return;
    if (m_model->mode() != CalibrationModel::Mode::Advanced) {
        m_advancedColumnGraph->setVisible(false);
        if (m_highlightGraph) m_highlightGraph->setVisible(false);
        ui->plotWidget->replot();
        return;
    }
    const QModelIndex curr = m_advancedTableView
                                 ? m_advancedTableView->currentIndex()
                                 : QModelIndex();
    const int col = curr.column();
    const int row = curr.row();
    const auto &rows = m_model->advancedRows();
    const auto &axis = m_model->advancedPowerAxis();
    QVector<double> xs, ys;
    if (col >= 0 && col < axis.size()) {
        xs.reserve(rows.size());
        ys.reserve(rows.size());
        // advancedRows is sorted by freq; iterating in order keeps the
        // polyline monotonic in X. Skip unset cells -- the polyline
        // bridges across the gap, which is the same convention as the
        // Simple plot's spline through populated points.
        for (const auto &row : rows) {
            if (col >= row.cells.size()) continue;
            const auto &cell = row.cells[col];
            if (!cell.isSet) continue;
            xs << row.frequencyMHz;
            ys << cell.correctionDb;
        }
        ui->plotWidget->yAxis->setLabel(
            tr("Correction at %1 dBm (dB)").arg(axis[col], 0, 'f', 1));
    } else {
        ui->plotWidget->yAxis->setLabel(tr("Correction (dB)"));
    }
    m_advancedColumnGraph->setData(xs, ys);
    m_advancedColumnGraph->setVisible(true);
    ui->plotWidget->rescaleAxes();
    if (!xs.isEmpty()) {
        QCPRange y = ui->plotWidget->yAxis->range();
        const double pad = qMax(y.size() * 0.1, 0.5);
        ui->plotWidget->yAxis->setRange(y.lower - pad, y.upper + pad);
    }

    // Highlight the selected freq point on the chart (same green
    // crosshair the Simple plot uses for its selection).
    if (m_highlightGraph) {
        const auto &rows = m_model->advancedRows();
        bool highlighted = false;
        if (row >= 0 && row < rows.size() && col >= 0
            && col < rows[row].cells.size() && rows[row].cells[col].isSet) {
            m_highlightGraph->setData({rows[row].frequencyMHz},
                                      {rows[row].cells[col].correctionDb});
            m_highlightGraph->setVisible(true);
            highlighted = true;
        }
        if (!highlighted) m_highlightGraph->setVisible(false);
    }
    ui->plotWidget->replot();
}
