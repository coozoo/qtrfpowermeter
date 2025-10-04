#include "calibrationmanager.h"
#include "ui_calibrationmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <numeric>

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
    connect(ui->generateButton, &QPushButton::clicked, this, &CalibrationManager::ongenerateButton_clicked);
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

    loadProfiles();
    setupPlot();
    onStartFreqChanged(ui->startFreqSpinBox->value());
    onEndFreqChanged(ui->endFreqSpinBox->value());
}

CalibrationManager::~CalibrationManager()
{
    delete ui;
}

void CalibrationManager::setupPlot()
{
    qDebug()<<Q_FUNC_INFO;
    ui->plotWidget->addGraph();
    ui->plotWidget->graph(0)->setPen(QPen(Qt::blue));
    ui->plotWidget->addGraph();
    ui->plotWidget->graph(1)->setPen(QPen(Qt::red));
    ui->plotWidget->graph(1)->setLineStyle(QCPGraph::lsNone);
    ui->plotWidget->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));

    ui->plotWidget->xAxis->setLabel("Frequency (MHz)");
    ui->plotWidget->yAxis->setLabel("Correction (dB)");
    ui->plotWidget->setInteractions({});
}

void CalibrationManager::updatePlot()
{
    qDebug()<<Q_FUNC_INFO;
    QVector<double> measuredFreqs, measuredCorrections;
    QVector<double> lineFreqs, lineCorrections;

    const auto &points = m_model->getPoints();
    if (points.isEmpty())
        {
            ui->plotWidget->graph(0)->setData({}, {});
            ui->plotWidget->graph(1)->setData({}, {});
            ui->plotWidget->replot();
            return;
        }

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
    else if (!points.isEmpty())
        {
            lineFreqs.append(minFreq);
            lineCorrections.append(m_model->getCorrection(minFreq));
        }

    ui->plotWidget->graph(0)->setData(lineFreqs, lineCorrections);
    ui->plotWidget->graph(1)->setData(measuredFreqs, measuredCorrections);
    ui->plotWidget->rescaleAxes();
    ui->plotWidget->replot();
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


void CalibrationManager::onNewMeasurement(double dbmValue)
{
    // If we are not in measurement mode, just ignore the data.
    if (m_samplesToTake <= 0)
        {
            return;
        }

    m_measurements.append(dbmValue);

    // Check if we have collected enough samples
    if (m_measurements.count() >= m_samplesToTake)
        {
            double sum = std::accumulate(m_measurements.begin(), m_measurements.end(), 0.0);
            double averageDbm = sum / m_measurements.count();

            double referenceDbm = ui->referencePowerSpinBox->value();
            double correction = referenceDbm - averageDbm;

            m_model->setData(m_model->index(m_selectedIndex.row(), CalibrationModel::Correction), correction, Qt::EditRole);
            QMessageBox::information(this, tr("Measurement Complete"), tr("Average measured power: %1 dBm\nCalculated correction: %2 dB").arg(averageDbm, 0, 'f', 2).arg(correction, 0, 'f', 2));
            m_samplesToTake = 0;
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
    m_model->generateFrequencies(startFreq, endFreq, stepFreq);
}

void CalibrationManager::ontable_clicked(const QModelIndex &index)
{
    qDebug()<<Q_FUNC_INFO;
    if (!index.isValid()) return;

    m_selectedIndex = index;
    double freq = m_model->data(m_model->index(index.row(), CalibrationModel::Frequency)).toDouble();
    ui->pickAverageButton->setEnabled(true);
    emit frequencySelected(freq);
}

void CalibrationManager::onpickAverageButton_clicked()
{
    qDebug()<<Q_FUNC_INFO;
    if (!m_selectedIndex.isValid())
        {
            QMessageBox::information(this, tr("No Selection"), tr("Please select a frequency in the table first."));
            return;
        }
    m_measurements.clear();
    m_samplesToTake = ui->sampleCountSpinBox->value();
}

double CalibrationManager::getCorrection(double frequencyMHz) const
{
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

// --- Profile Management ---

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
            QMessageBox::critical(this, tr("Error"), tr("Could not load profile '%1'.").arg(name));
            return;
        }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray pointsArray = doc.object()["points"].toArray();
    QVector<CalibrationPoint> points;
    for (const QJsonValue &val : pointsArray)
        {
            QJsonObject obj = val.toObject();
            CalibrationPoint p;
            p.frequencyMHz = obj["frequencyMHz"].toDouble();
            p.correctionDb = obj["correctionDb"].toDouble();
            p.isSet = obj["isSet"].toBool();
            points.append(p);
        }
    m_model->setPoints(points);
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

    QJsonArray pointsArray;
    for (const auto &point : m_model->getPoints())
        {
            QJsonObject pointObj;
            pointObj["frequencyMHz"] = point.frequencyMHz;
            pointObj["correctionDb"] = point.correctionDb;
            pointObj["isSet"] = point.isSet;
            pointsArray.append(pointObj);
        }

    QJsonObject rootObj;
    rootObj["name"] = name;
    rootObj["points"] = pointsArray;

    file.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
}
