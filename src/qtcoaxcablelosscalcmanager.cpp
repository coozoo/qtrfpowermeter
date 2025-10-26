#include "qtcoaxcablelosscalcmanager.h"
#include "cablemodel.h"
#include "cablewidget.h"
#include "3rdparty/qcustomplot/qcustomplot.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>
#include <QLabel>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCompleter>
#include <QStringListModel>
#include <QResizeEvent>
#include <QDebug>
#include <QRandomGenerator>

QtCoaxCableLossCalcManager::QtCoaxCableLossCalcManager(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupPlot();
}

QtCoaxCableLossCalcManager::~QtCoaxCableLossCalcManager()
{
    qDeleteAll(m_activeCableWidgets);
    m_activeCableWidgets.clear();
    qDeleteAll(m_cableModels);
    m_cableModels.clear();
    qDebug()<<"QtCoaxCableLossCalcManager destroyed.";
}

void QtCoaxCableLossCalcManager::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    QHBoxLayout *controlLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search for cable..."));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &QtCoaxCableLossCalcManager::onSearchTextChanged);

    m_cableComboBox = new QComboBox(this);
    m_cableComboBox->setMinimumWidth(200);

    m_addButton = new QPushButton(tr("Add"), this);
    m_addButton->setToolTip(tr("Add Cable"));
    connect(m_addButton, &QPushButton::clicked, this, &QtCoaxCableLossCalcManager::onAddCableClicked);

    m_deleteButton = new QPushButton(tr("Delete Marked"), this);
    connect(m_deleteButton, &QPushButton::clicked, this, &QtCoaxCableLossCalcManager::onDeleteMarkedClicked);

    m_clearAllButton = new QToolButton(this);
    m_clearAllButton->setToolTip(tr("Clear All"));
    m_clearAllButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_clearAllButton->setIcon(QIcon::fromTheme("edit-clear",QIcon(":/images/process-stop.svg")));
    connect(m_clearAllButton, &QToolButton::clicked, this, &QtCoaxCableLossCalcManager::onClearAllClicked);


    controlLayout->addWidget(m_searchEdit, 1);
    controlLayout->addWidget(m_cableComboBox, 2);
    controlLayout->addWidget(m_addButton);
    controlLayout->addWidget(m_deleteButton);
    controlLayout->addWidget(m_clearAllButton);
    controlLayout->addStretch();
    m_mainLayout->addLayout(controlLayout);

    m_plot = new QCustomPlot(this);
    m_mainLayout->addWidget(m_plot, 1);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::StyledPanel);

    m_scrollAreaWidget = new QWidget();
    m_cableWidgetsLayout = new QGridLayout(m_scrollAreaWidget);
    m_scrollArea->setWidget(m_scrollAreaWidget);
    m_mainLayout->addWidget(m_scrollArea, 1);

    QHBoxLayout *totalLayout = new QHBoxLayout();
    m_totalAttenuationLabel = new QLabel(this);
    m_totalAttenuationLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    totalLayout->addStretch();
    totalLayout->addWidget(m_totalAttenuationLabel);
    totalLayout->addStretch();
    m_mainLayout->addLayout(totalLayout);
}

void QtCoaxCableLossCalcManager::loadCablesFromJson(QString configPath)
{
    qDebug()<<"Loading cables override:"<<configPath;
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly))
        {
            qWarning()<<"Could not open" << configPath << "error:" << file.errorString();
            QMessageBox::critical(this, tr("Error"), tr("Could not open %1 error: %2").arg(configPath, file.errorString()));
            return;
        }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject rootObj = doc.object();

    QStringList cableNames;
    for (auto it = rootObj.begin(); it != rootObj.end(); ++it)
        {
            CableModel *model = new CableModel(it.value().toObject(), this);
            m_cableModels.insert(model->getName(), model);
            cableNames.append(model->getName());
        }

    cableNames.sort(Qt::CaseInsensitive);
    m_cableComboBox->addItems(cableNames);

    QCompleter *completer = new QCompleter(cableNames, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    m_searchEdit->setCompleter(completer);
}

void QtCoaxCableLossCalcManager::setupPlot()
{
    m_plot->xAxis->setLabel(tr("Frequency (MHz)"));
    m_plot->yAxis->setLabel(tr("Attenuation (dB/100m)"));
    m_plot->setInteractions({});
    m_plot->legend->setVisible(false);

    m_frequencyLine = new QCPItemStraightLine(m_plot);
    m_frequencyLine->setPen(QPen(Qt::red, 1, Qt::DashLine));
    m_frequencyLine->point1->setCoords(0, 0);
    m_frequencyLine->point2->setCoords(0, 1);

    connect(m_plot->xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &QtCoaxCableLossCalcManager::replotGraphs);

    m_plot->xAxis->setRange(0, 3000);
    m_plot->yAxis->setRange(0, 50);
    m_plot->replot();
}

void QtCoaxCableLossCalcManager::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateGrid();
}

void QtCoaxCableLossCalcManager::setSilentCableDupes(bool silent)
{
    m_silentCableDupes=silent;
    if(!m_allowCableDupes && m_silentCableDupes)
        connect(m_cableComboBox, &QComboBox::currentIndexChanged, this, &QtCoaxCableLossCalcManager::onCurrentCableIndexChanged);
}

void QtCoaxCableLossCalcManager::setPlotRange(double lower, double upper)
{
    if (m_plot)
        {
            m_plot->xAxis->setRange(lower, upper);
        }
}

void QtCoaxCableLossCalcManager::setIndividualLengthAllowed(bool allowed)
{
    m_individualLengthAllowed = allowed;
    for (CableWidget *widget : m_activeCableWidgets)
        {
            widget->setLengthEditable(allowed);
        }
    // If we just disabled individual lengths, reset all to the global value
    if (!allowed)
        {
            setGlobalLength(m_globalLength);
        }
}

void QtCoaxCableLossCalcManager::replotGraphs()
{
    for (int i = 0; i < m_plot->graphCount(); ++i)
        {
            QCPGraph *graph = m_plot->graph(i);
            if (!graph) continue;

            CableModel *model = m_cableModels.value(graph->name());
            if (!model) continue;

            QVector<double> freqs, losses;
            double minFreq = m_plot->xAxis->range().lower;
            double maxFreq = m_plot->xAxis->range().upper;
            int steps = 200;

            for (int j = 0; j <= steps; ++j)
                {
                    double freq = minFreq + j * (maxFreq - minFreq) / steps;
                    if (freq > 0)
                        {
                            freqs.append(freq);
                            losses.append(model->getAttenuationPer100m(freq));
                        }
                }
            graph->setData(freqs, losses, true);
        }

    m_plot->rescaleAxes(true);
    updateTracers();
    m_plot->replot();
}

void QtCoaxCableLossCalcManager::setFrequency(double frequencyMHz)
{
    if (m_plot->xAxis->range().upper < frequencyMHz)
        {
            m_plot->xAxis->setRangeUpper(frequencyMHz);
        }
    if (m_plot->xAxis->range().lower > frequencyMHz)
        {
            m_plot->xAxis->setRangeLower(frequencyMHz);
        }

    for (CableWidget *widget : m_activeCableWidgets)
        {
            widget->setFrequency(frequencyMHz);
        }
    m_frequencyLine->point1->setCoords(frequencyMHz, 0);
    m_frequencyLine->point2->setCoords(frequencyMHz, 1);

    updateTracers();
    m_plot->replot();
    updateAttenuations();
}

void QtCoaxCableLossCalcManager::updateTracers()
{
    double currentFreq = m_frequencyLine->point1->key();

    for (auto it = m_extrapolationTracers.begin(); it != m_extrapolationTracers.end(); ++it)
        {
            QCPGraph *graph = it.key();
            QCPItemTracer *tracer = it.value();
            CableModel *model = m_cableModels.value(graph->name());

            if (model && model->getMaxFrequency() > 0 && currentFreq > model->getMaxFrequency())
                {
                    tracer->setGraph(graph);
                    tracer->setGraphKey(currentFreq);
                    tracer->setPen(QPen(QColor("darkorange")));
                    tracer->setBrush(QBrush(QColor("darkorange")));
                    tracer->setVisible(true);
                }
            else if (model && model->getMinFrequency() > 0 && currentFreq < model->getMinFrequency())
                {
                    tracer->setGraph(graph);
                    tracer->setGraphKey(currentFreq);
                    tracer->setPen(QPen(QColor("lightblue")));
                    tracer->setBrush(QBrush(QColor("lightblue")));
                    tracer->setVisible(true);
                }
            else
                {
                    tracer->setVisible(false);
                }
        }
}

void QtCoaxCableLossCalcManager::setGlobalLength(double lengthM)
{
    m_globalLength = lengthM;
    for (CableWidget *widget : m_activeCableWidgets)
        {
            widget->setLength(lengthM);
        }
}

void QtCoaxCableLossCalcManager::onAddCableClicked()
{
    QString cableName = m_cableComboBox->currentText();
    if (m_cableModels.contains(cableName))
        {
            for (CableWidget *widget : m_activeCableWidgets)
            {
                if(!m_allowCableDupes && widget->getModel()->getName()==cableName)
                {
                    QMessageBox::warning(this, tr("Warning"), tr("Already added!"));
                    return;
                }
            }
            CableModel *model = m_cableModels.value(cableName);
            CableWidget *widget = new CableWidget(model);

            widget->setLengthEditable(m_individualLengthAllowed);
            widget->setLength(m_globalLength);

            widget->setFrequency(m_frequencyLine->point1->key());
            connect(widget, &CableWidget::attenuationChanged, this, &QtCoaxCableLossCalcManager::updateAttenuations);

            m_activeCableWidgets.append(widget);
            addCableToPlot(widget);
            updateGrid();
            updateAttenuations();
        }
    m_plot->legend->setVisible(m_activeCableWidgets.count() > 0);
    m_plot->replot();
    if(!m_allowCableDupes && m_silentCableDupes) onCurrentCableIndexChanged(0);
}

void QtCoaxCableLossCalcManager::onDeleteMarkedClicked()
{
    QList<CableWidget *> widgetsToDelete;
    for (CableWidget *widget : m_activeCableWidgets)
        {
            if (widget->isMarkedForDeletion())
                {
                    widgetsToDelete.append(widget);
                }
        }

    if (widgetsToDelete.isEmpty()) return;

    for (CableWidget *widget : widgetsToDelete)
        {
            m_activeCableWidgets.removeAll(widget);
            removeCableFromPlot(widget);
            widget->deleteLater();
        }
    m_plot->legend->setVisible(m_activeCableWidgets.count() > 0);
    m_plot->replot();
    updateGrid();
    updateAttenuations();
}

void QtCoaxCableLossCalcManager::onClearAllClicked()
{
    m_plot->legend->setVisible(false);
    for (CableWidget *widget : m_activeCableWidgets)
    {
        removeCableFromPlot(widget);
        widget->deleteLater();
    }
    m_activeCableWidgets.clear();
    updateGrid();
    updateAttenuations();
}

void QtCoaxCableLossCalcManager::onSearchTextChanged(const QString &text)
{
    QStringList foundItems;
    for (CableModel *model : m_cableModels.values())
        {
            if (text.isEmpty() ||
                    model->getName().contains(text, Qt::CaseInsensitive) ||
                    model->getManufacturer().contains(text, Qt::CaseInsensitive) ||
                    model->getType().contains(text, Qt::CaseInsensitive))
                {
                    foundItems.append(model->getName());
                }
        }
    foundItems.sort(Qt::CaseInsensitive);
    m_cableComboBox->clear();
    m_cableComboBox->addItems(foundItems);
}

void QtCoaxCableLossCalcManager::updateAttenuations()
{
    double total = 0.0;
    for (CableWidget *widget : m_activeCableWidgets)
        {
            total += widget->getCurrentAttenuation();
        }
    m_totalAttenuationLabel->setText(tr("Total Attenuation: %1 dB").arg(total, 0, 'f', 2));
    emit totalAttenuationChanged(total);
}


void QtCoaxCableLossCalcManager::addCableToPlot(CableWidget *cableWidget)
{
    QCPGraph *graph = m_plot->addGraph();
    graph->setName(cableWidget->getModel()->getName());
    QPen pen;
    //pen.setColor(QColor::fromHsv(QRandomGenerator::global()->bounded(360),200, 200));
    pen.setColor(uniColor[m_nextColorIndex]);
    m_nextColorIndex = (m_nextColorIndex + 1) % uniColor.size();
    pen.setWidth(2);
    graph->setPen(pen);

    QCPItemTracer *tracer = new QCPItemTracer(m_plot);
    tracer->setPen(QPen(QColor("darkorange")));
    tracer->setBrush(QBrush(QColor("darkorange")));
    tracer->setStyle(QCPItemTracer::tsCircle);
    tracer->setSize(7);
    tracer->setVisible(false);
    m_extrapolationTracers.insert(graph, tracer);

    replotGraphs();
    updateTracers();
    m_plot->rescaleAxes(true);
    m_plot->replot();
}

void QtCoaxCableLossCalcManager::removeCableFromPlot(CableWidget *cableWidget)
{
    for (int i = 0; i < m_plot->graphCount(); ++i)
        {
            QCPGraph *graph = m_plot->graph(i);
            if (graph && graph->name() == cableWidget->getModel()->getName())
                {
                    if (m_extrapolationTracers.contains(graph))
                        {
                            m_plot->removeItem(m_extrapolationTracers.value(graph));
                            m_extrapolationTracers.remove(graph);
                        }
                    m_plot->removeGraph(graph);
                    break;
                }
        }
    m_plot->rescaleAxes(true);
    m_plot->replot();
}

void QtCoaxCableLossCalcManager::updateGrid()
{
     while (QLayoutItem *item = m_cableWidgetsLayout->takeAt(0))
        {
            delete item;
        }

    int widgetWidth = 250;
    int scrollAreaWidth = m_scrollArea->viewport()->width();
    int columnCount = qMax(1, scrollAreaWidth / (widgetWidth + m_cableWidgetsLayout->horizontalSpacing()));

    for (int i = 0; i < m_activeCableWidgets.size(); ++i)
        {
            int row = i / columnCount;
            int col = i % columnCount;
            m_cableWidgetsLayout->addWidget(m_activeCableWidgets[i], row, col, Qt::AlignTop | Qt::AlignLeft);
        }

    for (int i = 0; i < m_cableWidgetsLayout->columnCount(); ++i)
        {
            m_cableWidgetsLayout->setColumnStretch(i, 0);
        }
    for (int i = 0; i < m_cableWidgetsLayout->rowCount(); ++i)
        {
            m_cableWidgetsLayout->setRowStretch(i, 0);
        }

    m_cableWidgetsLayout->setColumnStretch(m_cableWidgetsLayout->columnCount(), 1);
    m_cableWidgetsLayout->setRowStretch(m_cableWidgetsLayout->rowCount(), 1);
}

void QtCoaxCableLossCalcManager::onCurrentCableIndexChanged(int)
{
    QString cableName = m_cableComboBox->currentText();
    if (m_cableModels.contains(cableName))
    {
        for (CableWidget *widget : m_activeCableWidgets)
        {
            if(!m_allowCableDupes && widget->getModel()->getName()==cableName)
            {
                m_addButton->setDisabled(true);
                m_addButton->setToolTip(tr("Current Cable Already Added"));
                return;
            }
            else
            {
                m_addButton->setDisabled(false);
                m_addButton->setToolTip(tr("Add Cable"));
            }
        }
    }
}
