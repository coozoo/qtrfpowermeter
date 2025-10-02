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

    // Connect UI signals to slots
    connect(ui->generateButton, &QPushButton::clicked, this, &CalibrationManager::on_generateButton_clicked);
    connect(ui->pickAverageButton, &QPushButton::clicked, this, &CalibrationManager::on_pickAverageButton_clicked);
    connect(ui->saveProfileButton, &QPushButton::clicked, this, &CalibrationManager::on_saveProfileButton_clicked);
    connect(ui->loadProfileButton, &QPushButton::clicked, this, &CalibrationManager::on_loadProfileButton_clicked);
    connect(ui->deleteProfileButton, &QPushButton::clicked, this, &CalibrationManager::on_deleteProfileButton_clicked);
    connect(ui->profileComboBox, &QComboBox::currentTextChanged, this, &CalibrationManager::on_profileComboBox_currentIndexChanged);
    connect(ui->tableView, &QTableView::clicked, this, &CalibrationManager::on_table_clicked);

    // Setup unit comboboxes
    ui->startUnitComboBox->addItems({"MHz", "GHz"});
    ui->endUnitComboBox->addItems({"MHz", "GHz"});
    ui->stepUnitComboBox->addItems({"MHz", "GHz"});

    loadProfiles();
}

CalibrationManager::~CalibrationManager()
{
    delete ui;
}

void CalibrationManager::onNewMeasurement(double dbmValue)
{
    // If we are not in measurement mode, just ignore the data.
    if (m_samplesToTake <= 0) {
        return;
    }

    m_measurements.append(dbmValue);

    // Check if we have collected enough samples
    if (m_measurements.count() >= m_samplesToTake) {
        double sum = std::accumulate(m_measurements.begin(), m_measurements.end(), 0.0);
        double averageDbm = sum / m_measurements.count();

        double referenceDbm = ui->referencePowerSpinBox->value();
        double correction = referenceDbm - averageDbm;

        m_model->setData(m_model->index(m_selectedIndex.row(), CalibrationModel::Correction), correction, Qt::EditRole);
        QMessageBox::information(this, tr("Measurement Complete"), tr("Average measured power: %1 dBm\nCalculated correction: %2 dB").arg(averageDbm, 0, 'f', 2).arg(correction, 0, 'f', 2));

        m_samplesToTake = 0; // Reset the state
    }
}


void CalibrationManager::on_generateButton_clicked()
{
    double startFreq = ui->startFreqSpinBox->value();
    if (ui->startUnitComboBox->currentText() == "GHz") startFreq *= 1000.0;

    double endFreq = ui->endFreqSpinBox->value();
    if (ui->endUnitComboBox->currentText() == "GHz") endFreq *= 1000.0;

    double stepFreq = ui->stepFreqSpinBox->value();
    if (ui->stepUnitComboBox->currentText() == "GHz") stepFreq *= 1000.0;

    if (stepFreq <= 0) {
        QMessageBox::warning(this, tr("Invalid Step"), tr("Frequency step must be greater than zero."));
        return;
    }
    m_model->generateFrequencies(startFreq, endFreq, stepFreq);
}

void CalibrationManager::on_table_clicked(const QModelIndex &index)
{
    if (!index.isValid()) return;

    m_selectedIndex = index;
    double freq = m_model->data(m_model->index(index.row(), CalibrationModel::Frequency)).toDouble();
    ui->pickAverageButton->setEnabled(true);
    emit frequencySelected(freq);
}

void CalibrationManager::on_pickAverageButton_clicked()
{
    if (!m_selectedIndex.isValid()) {
        QMessageBox::information(this, tr("No Selection"), tr("Please select a frequency in the table first."));
        return;
    }
    m_measurements.clear();
    m_samplesToTake = ui->sampleCountSpinBox->value(); // Set internal state
}

double CalibrationManager::getCorrection(double frequencyMHz) const
{
    return m_model->getCorrection(frequencyMHz);
}

void CalibrationManager::setActiveProfile(const QString &name)
{
    if (ui->profileComboBox->findText(name) != -1) {
        ui->profileComboBox->setCurrentText(name);
    }
}

// --- Profile Management ---

QString CalibrationManager::getProfilesPath() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir dir(path);
    if (!dir.exists("calibration")) {
        dir.mkdir("calibration");
    }
    return path + "/calibration";
}

void CalibrationManager::loadProfiles()
{
    ui->profileComboBox->clear();
    QDir dir(getProfilesPath());
    QStringList filters;
    filters << "*.json";
    QFileInfoList list = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);

    for (const QFileInfo &fileInfo : list) {
        ui->profileComboBox->addItem(fileInfo.baseName());
    }
}

void CalibrationManager::on_saveProfileButton_clicked()
{
    QString name = ui->profileNameLineEdit->text();
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Invalid Name"), tr("Please enter a profile name."));
        return;
    }
    saveProfile(name);
    if (ui->profileComboBox->findText(name) == -1) {
        ui->profileComboBox->addItem(name);
    }
    ui->profileComboBox->setCurrentText(name);
    QMessageBox::information(this, tr("Success"), tr("Profile '%1' saved.").arg(name));
}

void CalibrationManager::on_loadProfileButton_clicked()
{
    QString name = ui->profileComboBox->currentText();
    if (name.isEmpty()) return;

    QFile file(getProfilesPath() + "/" + name + ".json");
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not load profile '%1'.").arg(name));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray pointsArray = doc.object()["points"].toArray();
    QVector<CalibrationPoint> points;
    for (const QJsonValue &val : pointsArray) {
        QJsonObject obj = val.toObject();
        CalibrationPoint p;
        p.frequencyMHz = obj["frequencyMHz"].toDouble();
        p.correctionDb = obj["correctionDb"].toDouble();
        p.isSet = obj["isSet"].toBool();
        points.append(p);
    }
    m_model->setPoints(points);
    ui->profileNameLineEdit->setText(name);
}

void CalibrationManager::on_deleteProfileButton_clicked()
{
    QString name = ui->profileComboBox->currentText();
    if (name.isEmpty()) return;

    if (QMessageBox::question(this, tr("Confirm Delete"), tr("Are you sure you want to delete the profile '%1'?").arg(name)) == QMessageBox::Yes) {
        QFile file(getProfilesPath() + "/" + name + ".json");
        if (file.remove()) {
            QMessageBox::information(this, tr("Success"), tr("Profile '%1' deleted.").arg(name));
            loadProfiles();
        } else {
            QMessageBox::critical(this, tr("Error"), tr("Could not delete profile '%1'.").arg(name));
        }
    }
}

void CalibrationManager::on_profileComboBox_currentIndexChanged(const QString &name)
{
    if (!name.isEmpty()) {
        emit currentProfileChanged(name);
    }
}

void CalibrationManager::saveProfile(const QString &name)
{
    QFile file(getProfilesPath() + "/" + name + ".json");
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Could not open file for writing:" << file.fileName();
        return;
    }

    QJsonArray pointsArray;
    for (const auto &point : m_model->getPoints()) {
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
