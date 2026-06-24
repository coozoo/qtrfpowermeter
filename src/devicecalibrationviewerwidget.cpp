#include "devicecalibrationviewerwidget.h"
#include "ui_devicecalibrationviewerwidget.h"
#include "conceptrfrpmdevice.h"
#include "conceptrfrpmlookuptables.h"
#include "savedtoast.h"

#include <QStandardItemModel>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QMessageBox>
#include <QPen>
#include <QColor>

#include "qcustomplot.h"

DeviceCalibrationViewerWidget::DeviceCalibrationViewerWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DeviceCalibrationViewerWidget)
    , m_tableModel(nullptr)
    , m_curve(nullptr)
{
    ui->setupUi(this);

    setupPlot();

    connect(ui->exportCsvButton, &QPushButton::clicked,
            this, &DeviceCalibrationViewerWidget::onExportCsvClicked);

    clear();
}

DeviceCalibrationViewerWidget::~DeviceCalibrationViewerWidget()
{
    delete ui;
}

void DeviceCalibrationViewerWidget::clear()
{
    m_modelName.clear();
    m_firmwareVersion.clear();
    m_serialNumber.clear();
    m_freqAxisHz.clear();
    m_powerAxisDb.clear();
    m_voltageTableMv.clear();

    ui->modelValueLabel->setText("-");
    ui->fwValueLabel->setText("-");
    ui->snValueLabel->setText("-");
    ui->dimsValueLabel->setText("-");
    ui->statusValueLabel->setText(tr("No calibration table available."));
    ui->exportCsvButton->setEnabled(false);

    if (m_tableModel) {
        m_tableModel->clear();
    }
    if (m_curve) {
        m_curve->setData(QVector<double>(), QVector<double>());
        ui->plotWidget->replot();
    }
}

void DeviceCalibrationViewerWidget::setDevice(const ConceptRfRpmDevice *device)
{
    if (!device || !device->lookupTable()) {
        clear();
        return;
    }

    m_modelName       = device->properties().name;
    m_firmwareVersion = QString::number(device->firmwareVersion(), 'f', 2);
    m_serialNumber    = QString::number(device->serialNumber());

    const AbstractRpmLookupTable *t = device->lookupTable();
    m_freqAxisHz     = t->freqAxisHz();
    m_powerAxisDb    = t->powerAxisDb();
    m_voltageTableMv = t->voltageTableMv();

    ui->modelValueLabel->setText(m_modelName.isEmpty() ? tr("(unknown)") : m_modelName);
    ui->fwValueLabel->setText(m_firmwareVersion);
    ui->snValueLabel->setText(m_serialNumber);
    ui->dimsValueLabel->setText(tr("%1 frequencies x %2 power levels")
                                .arg(m_freqAxisHz.size())
                                .arg(m_powerAxisDb.size()));

    int filled = 0;
    for (int i = 0; i < m_voltageTableMv.size(); ++i) {
        if (t->isRowFilled(i)) ++filled;
    }
    if (filled == m_freqAxisHz.size()) {
        ui->statusValueLabel->setText(tr("All %1 rows filled.").arg(filled));
    } else {
        ui->statusValueLabel->setText(tr("%1 of %2 rows filled (partial).")
                                      .arg(filled).arg(m_freqAxisHz.size()));
    }

    populateTable();

    ui->exportCsvButton->setEnabled(true);

    if (m_tableModel && m_tableModel->rowCount() > 0) {
        ui->tableView->selectRow(0);
        plotRow(0);
    }
}

QString DeviceCalibrationViewerWidget::formatFreq(quint64 hz) const
{
    if (hz >= 1000000000ULL) {
        return QString::number(hz / 1e9, 'f', 3) + " GHz";
    } else if (hz >= 1000000ULL) {
        return QString::number(hz / 1e6, 'f', 3) + " MHz";
    } else if (hz >= 1000ULL) {
        return QString::number(hz / 1e3, 'f', 3) + " kHz";
    }
    return QString::number(hz) + " Hz";
}

void DeviceCalibrationViewerWidget::populateTable()
{
    if (!m_tableModel) {
        m_tableModel = new QStandardItemModel(this);
        ui->tableView->setModel(m_tableModel);
        connect(ui->tableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
                this, &DeviceCalibrationViewerWidget::onRowSelectionChanged);
    }
    m_tableModel->clear();
    m_tableModel->setRowCount(m_freqAxisHz.size());
    m_tableModel->setColumnCount(m_powerAxisDb.size());

    QStringList colHeaders;
    colHeaders.reserve(m_powerAxisDb.size());
    for (float dbm : m_powerAxisDb) {
        colHeaders << QString::number(static_cast<double>(dbm), 'f', 1);
    }
    m_tableModel->setHorizontalHeaderLabels(colHeaders);

    QStringList rowHeaders;
    rowHeaders.reserve(m_freqAxisHz.size());
    for (quint64 hz : m_freqAxisHz) {
        rowHeaders << formatFreq(hz);
    }
    m_tableModel->setVerticalHeaderLabels(rowHeaders);

    for (int r = 0; r < m_voltageTableMv.size(); ++r) {
        const QVector<float> &row = m_voltageTableMv[r];
        const int cols = qMin(row.size(), m_powerAxisDb.size());
        for (int c = 0; c < cols; ++c) {
            QStandardItem *item = new QStandardItem();
            item->setData(QString::number(static_cast<double>(row[c]), 'f', 3),
                          Qt::DisplayRole);
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            item->setEditable(false);
            m_tableModel->setItem(r, c, item);
        }
        for (int c = cols; c < m_powerAxisDb.size(); ++c) {
            QStandardItem *item = new QStandardItem(QStringLiteral("-"));
            item->setTextAlignment(Qt::AlignCenter);
            item->setEditable(false);
            m_tableModel->setItem(r, c, item);
        }
    }

    // Use Interactive sizing so the table doesn't request a giant
    // sizeHint (21 columns x ResizeToContents would push the embedding
    // CalibrationManager dock and the MainWindow off-screen). Scroll
    // bars cover the rest.
    ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->tableView->horizontalHeader()->setDefaultSectionSize(60);
    ui->tableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    ui->tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
}

void DeviceCalibrationViewerWidget::setupPlot()
{
    QCustomPlot *p = ui->plotWidget;
    p->addGraph();
    m_curve = p->graph(0);
    m_curve->setPen(QPen(Qt::blue));
    m_curve->setLineStyle(QCPGraph::lsLine);
    m_curve->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4));

    p->xAxis->setLabel(tr("Power (dBm)"));
    p->yAxis->setLabel(tr("Voltage (mV)"));
}

void DeviceCalibrationViewerWidget::plotRow(int freqIndex)
{
    if (freqIndex < 0 || freqIndex >= m_voltageTableMv.size()) return;

    const QVector<float> &row = m_voltageTableMv[freqIndex];
    QVector<double> xs, ys;
    const int cols = qMin(row.size(), m_powerAxisDb.size());
    xs.reserve(cols);
    ys.reserve(cols);
    for (int c = 0; c < cols; ++c) {
        xs << static_cast<double>(m_powerAxisDb[c]);
        ys << static_cast<double>(row[c]);
    }
    m_curve->setData(xs, ys);

    const QString freqText = (freqIndex < m_freqAxisHz.size())
                                 ? formatFreq(m_freqAxisHz[freqIndex])
                                 : QString();
    ui->plotGroupBox->setTitle(tr("Voltage vs power at %1").arg(freqText));

    ui->plotWidget->rescaleAxes();
    ui->plotWidget->replot();
}

void DeviceCalibrationViewerWidget::onRowSelectionChanged(const QModelIndex &current,
                                                          const QModelIndex &previous)
{
    Q_UNUSED(previous);
    if (!current.isValid()) return;
    plotRow(current.row());
}

void DeviceCalibrationViewerWidget::onExportCsvClicked()
{
    if (m_voltageTableMv.isEmpty()) return;

    const QString defaultName = QString("device_calibration_%1_%2.csv")
                                    .arg(m_modelName.isEmpty() ? "device" : m_modelName)
                                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    const QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString suggested = QDir(defaultDir).filePath(defaultName);

    const QString path = QFileDialog::getSaveFileName(this,
                                                      tr("Export device calibration"),
                                                      suggested,
                                                      tr("CSV files (*.csv)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export failed"),
                             tr("Could not open %1 for writing.").arg(path));
        return;
    }

    QTextStream out(&file);
    out << "# Device: " << m_modelName
        << ", FW " << m_firmwareVersion
        << ", S/N " << m_serialNumber << "\n";
    out << "# Voltage values are in mV. Rows are frequencies, columns are power levels in dBm.\n";

    out << "Frequency (Hz)";
    for (float dbm : m_powerAxisDb) {
        out << "," << QString::number(static_cast<double>(dbm), 'f', 2);
    }
    out << "\n";

    for (int r = 0; r < m_voltageTableMv.size(); ++r) {
        out << (r < m_freqAxisHz.size() ? QString::number(m_freqAxisHz[r]) : QString());
        const QVector<float> &row = m_voltageTableMv[r];
        const int cols = qMin(row.size(), m_powerAxisDb.size());
        for (int c = 0; c < cols; ++c) {
            out << "," << QString::number(static_cast<double>(row[c]), 'f', 4);
        }
        for (int c = cols; c < m_powerAxisDb.size(); ++c) {
            out << ",";
        }
        out << "\n";
    }

    file.close();
    notify::showSavedToast(ui->exportCsvButton, tr("Saved %1").arg(QFileInfo(path).fileName()), path);
}
