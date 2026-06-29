#include "attenuationmanager.h"
#include "attenuatorwidget.h"
#include <QMimeData>
#include <cmath>


AttenuationManager::AttenuationManager(QWidget *parent)
    : QWidget(parent)
    , m_currentFreqHz(std::numeric_limits<double>::quiet_NaN())
    , m_probeInputDbm(std::numeric_limits<double>::quiet_NaN())
    , m_deviceMaxInputDbm(std::numeric_limits<double>::quiet_NaN())
{
    setupUi();
}

void AttenuationManager::setCurrentFrequencyMHz(double freqMHz)
{
    const double hz = freqMHz * 1.0e6;
    if (qFuzzyCompare(m_currentFreqHz + 1.0, hz + 1.0)) return;
    m_currentFreqHz = hz;
    for (AttenuatorWidget *w : m_attenuatorWidgets)
        w->setCurrentFrequencyHz(m_currentFreqHz);
    reevaluateChain();
}

void AttenuationManager::setProbeInputDbm(double inputDbm)
{
    if (qFuzzyCompare(m_probeInputDbm + 1.0, inputDbm + 1.0)) return;
    m_probeInputDbm = inputDbm;
    reevaluateChain();
}

void AttenuationManager::setDeviceMaxInputDbm(double deviceMaxInputDbm)
{
    // NaN-aware compare: both NaN means "unchanged".
    const bool bothNan = std::isnan(m_deviceMaxInputDbm) && std::isnan(deviceMaxInputDbm);
    if (bothNan) return;
    if (!std::isnan(m_deviceMaxInputDbm) && !std::isnan(deviceMaxInputDbm)
        && qFuzzyCompare(m_deviceMaxInputDbm + 1.0, deviceMaxInputDbm + 1.0))
        return;
    m_deviceMaxInputDbm = deviceMaxInputDbm;
    reevaluateChain();
}

QList<StageInfo> AttenuationManager::currentStages() const
{
    QList<StageInfo> stages;
    stages.reserve(m_attenuatorWidgets.size());
    for (AttenuatorWidget *w : m_attenuatorWidgets)
        {
            const bool isInternal = (w == m_internalAttenuatorWidget);
            const double rating = isInternal ? m_deviceMaxInputDbm : w->maxInputDbm();
            stages.append(StageInfo{
                w->descriptionText(),
                w->getAttenuation(),
                rating,
                isInternal
            });
        }
    return stages;
}

void AttenuationManager::reevaluateChain()
{
    if (std::isnan(m_probeInputDbm)) return; // no input known yet, nothing to flag
    const ChainReport report = ChainSafetyEvaluator::evaluate(m_probeInputDbm, currentStages());
    // Per-plate fan-out so each widget shows its own state.
    for (int i = 0; i < report.perStage.size() && i < m_attenuatorWidgets.size(); ++i)
        m_attenuatorWidgets.at(i)->setOverloadState(report.perStage.at(i));
    emit safetyStateChanged(report);
}

AttenuationManager::~AttenuationManager()
{
    // Widgets are owned via Qt's parent chain (scrollArea -> m_scrollContent
    // -> AttenuatorWidget); the base ~QWidget cascade disposes of them.
    // No manual qDeleteAll needed.
}

void AttenuationManager::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // --- Top Control Panel ---
    QHBoxLayout *controlLayout = new QHBoxLayout();
    m_typeComboBox = new QComboBox();
    QIcon fixedIcon(":/images/AttenuatorRF.svg");
    QIcon digitalIcon(":/images/digiattcontrol.svg");
    QIcon cableIcon(":/images/coaxcable.svg");

    m_typeComboBox->addItem(fixedIcon, tr("Fixed Attenuator"), AttenuatorWidget::Fixed);
    m_typeComboBox->addItem(digitalIcon, tr("Digital Attenuator"), AttenuatorWidget::Digital);
    m_typeComboBox->addItem(cableIcon, tr("Cable Loss"), AttenuatorWidget::Cable);

    QPushButton *addButton = new QPushButton(tr("Add"));
    addButton->setToolTip(tr("Add Attenuator"));
    addButton->setIcon(QIcon::fromTheme(QStringLiteral("list-add"),
                                        QIcon(QStringLiteral(":/images/list-add.svg"))));
    QPushButton *removeButton = new QPushButton(tr("Remove"));
    removeButton->setToolTip(tr("Remove Selected"));
    removeButton->setIcon(QIcon::fromTheme(QStringLiteral("process-stop"),
                                           QIcon(QStringLiteral(":/images/process-stop.svg"))));

    controlLayout->addWidget(m_typeComboBox);
    controlLayout->addWidget(addButton);
    controlLayout->addWidget(removeButton);
    controlLayout->addStretch();

    // --- Scroll Area for the list of widgets ---
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    m_scrollContent = new QWidget();
    m_scrollContent->setAcceptDrops(true); // we route via the manager's overrides below
    m_listLayout = new QVBoxLayout(m_scrollContent);
    m_listLayout->addStretch();

    scrollArea->setWidget(m_scrollContent);
    setAcceptDrops(true);

    QHBoxLayout *totalLayout = new QHBoxLayout();

    // Total dB display widgets
    m_totalLcd = new QLCDNumber();
    m_totalLcd->setSegmentStyle(QLCDNumber::Flat);
    m_totalLcd->setDigitCount(6);

    // Assemble all widgets into the single horizontal layout
    totalLayout->addStretch();
    totalLayout->addWidget(new QLabel(tr("Total Attenuation:")));
    totalLayout->addWidget(m_totalLcd);
    totalLayout->addWidget(new QLabel("dB"));

    // Section break helpers: thin horizontal rule + bold centred label so
    // the chain reads top-to-bottom in physical signal order.
    auto makeSection = [this](const QString &text) -> QVBoxLayout * {
        QVBoxLayout *box = new QVBoxLayout();
        box->setSpacing(2);
        box->setContentsMargins(0, 4, 0, 4);

        QFrame *ruleTop = new QFrame(this);
        ruleTop->setFrameShape(QFrame::HLine);
        ruleTop->setFrameShadow(QFrame::Sunken);

        QLabel *label = new QLabel(text, this);
        label->setAlignment(Qt::AlignCenter);
        QFont f = label->font();
        f.setBold(true);
        label->setFont(f);

        QFrame *ruleBottom = new QFrame(this);
        ruleBottom->setFrameShape(QFrame::HLine);
        ruleBottom->setFrameShadow(QFrame::Sunken);

        box->addWidget(ruleTop);
        box->addWidget(label);
        box->addWidget(ruleBottom);
        return box;
    };

    mainLayout->addLayout(controlLayout);
    mainLayout->addLayout(makeSection(tr("▼ INPUT (from source)")));
    mainLayout->addWidget(scrollArea);
    mainLayout->addLayout(makeSection(tr("▼ OUTPUT (to meter)")));
    mainLayout->addLayout(totalLayout);

    connect(addButton, &QPushButton::clicked, this, &AttenuationManager::addAttenuator);
    connect(removeButton, &QPushButton::clicked, this, &AttenuationManager::removeSelectedAttenuators);
}

void AttenuationManager::addAttenuator()
{
    AttenuatorWidget::AttenuatorType type = (AttenuatorWidget::AttenuatorType)m_typeComboBox->currentData().toInt();
    AttenuatorWidget *newWidget = new AttenuatorWidget(type);

    // Connect the widget's valueChanged signal to our update slot
    connect(newWidget, &AttenuatorWidget::valueChanged, this, &AttenuationManager::updateTotalAttenuation);
    connect(newWidget, &QGroupBox::toggled, this, &AttenuationManager::updateTotalAttenuation); // Also update if enabled/disabled
    // Rating changes (Fixed user edit, Digital re-detection) don't shift
    // total attenuation but do change chain safety; re-evaluate explicitly.
    connect(newWidget, &AttenuatorWidget::maxInputDbmChanged, this, [this](double){ reevaluateChain(); });

    // If it's a cable widget, emit it so MainWindow can connect frequency signals
    if (type == AttenuatorWidget::Cable) {
        emit cableManagerAdded(newWidget->cableManager());
    }

    // Pin invariant: nothing lands at or below the device's own front-end
    // stage. Drag-and-drop already enforces this via clampForInternal; new
    // widgets created from the Add button must respect it too, otherwise
    // chain-safety math treats the external plate as downstream of internal.
    int listInsertIdx = m_attenuatorWidgets.size();
    int layoutInsertIdx = m_listLayout->count() - 1; // slot just before the stretch
    if (m_internalAttenuatorWidget) {
        const int internalListIdx = m_attenuatorWidgets.indexOf(m_internalAttenuatorWidget);
        const int internalLayoutIdx = m_listLayout->indexOf(m_internalAttenuatorWidget);
        if (internalListIdx >= 0) listInsertIdx = internalListIdx;
        if (internalLayoutIdx >= 0) layoutInsertIdx = internalLayoutIdx;
    }
    m_attenuatorWidgets.insert(listInsertIdx, newWidget);
    m_listLayout->insertWidget(layoutInsertIdx, newWidget);

    if (!std::isnan(m_currentFreqHz))
        newWidget->setCurrentFrequencyHz(m_currentFreqHz);

    qDebug()<<"Added new attenuator. Total count:"<<m_attenuatorWidgets.size();

    updateTotalAttenuation();
}

void AttenuationManager::removeSelectedAttenuators()
{
    qDebug()<<Q_FUNC_INFO;
    // Use a reverse iterator to safely remove items while looping
    for (int i = m_attenuatorWidgets.count() - 1; i >= 0; --i)
    {
        AttenuatorWidget *widget = m_attenuatorWidgets[i];
        if (widget->isMarkedForRemoval())
        {
            // If it's a cable widget, emit a signal so MainWindow can disconnect
            if (widget->cableManager()) {
                emit cableManagerRemoved(widget->cableManager());
            }

            // Remove from our list of pointers
            m_attenuatorWidgets.removeAt(i);

            // Remove from the visual layout
            m_listLayout->removeWidget(widget);

            // Schedule the widget for deletion
            widget->deleteLater();
        }
    }
    qDebug()<<"Removed selected. Total count:"<<m_attenuatorWidgets.count();
    updateTotalAttenuation();
}

void AttenuationManager::updateTotalAttenuation()
{
    qDebug()<<Q_FUNC_INFO;
    double total = 0.0;
    // Loop over the list of pointers
    for (AttenuatorWidget *widget : m_attenuatorWidgets)
    {
        total += widget->getAttenuation();
    }

    m_totalLcd->display(total);
    emit totalAttenuationChanged(total);
    reevaluateChain();
}

void AttenuationManager::addInternalAttenuator(double min, double max, double step,
                                               double deviceMaxInputDbm)
{
    if (m_internalAttenuatorWidget) return;

    // The internal-stage rating is also kept on the manager for the chain
    // evaluator so it survives subsequent addInternalAttenuator/remove
    // cycles even if the caller forgets to pass it again.
    if (!std::isnan(deviceMaxInputDbm))
        m_deviceMaxInputDbm = deviceMaxInputDbm;

    m_internalAttenuatorWidget = new AttenuatorWidget(AttenuatorWidget::InternalDigital);
    m_internalAttenuatorWidget->setInternalProperties(min, max, step, m_deviceMaxInputDbm); // Pass properties down

    // This signal tells MainWindow when the value is changed *from within this widget's editor*
    connect(m_internalAttenuatorWidget, &AttenuatorWidget::valueChanged, this, &AttenuationManager::internalAttenuationChanged);

    // This updates the total when the value changes
    connect(m_internalAttenuatorWidget, &AttenuatorWidget::valueChanged, this, &AttenuationManager::updateTotalAttenuation);

    // Pinned at the bottom (signal flows top -> bottom: source through
    // external attenuators into the device's own front-end, then to the
    // meter). Insert just before the trailing stretch.
    m_attenuatorWidgets.append(m_internalAttenuatorWidget);
    m_listLayout->insertWidget(m_listLayout->count() - 1, m_internalAttenuatorWidget);

    if (!std::isnan(m_currentFreqHz))
        m_internalAttenuatorWidget->setCurrentFrequencyHz(m_currentFreqHz);

    updateTotalAttenuation();
}

void AttenuationManager::removeInternalAttenuator()
{
    if (!m_internalAttenuatorWidget) {
        return;
    }

    m_attenuatorWidgets.removeAll(m_internalAttenuatorWidget);
    m_listLayout->removeWidget(m_internalAttenuatorWidget);
    m_internalAttenuatorWidget->deleteLater();
    m_internalAttenuatorWidget = nullptr;

    updateTotalAttenuation();
}

void AttenuationManager::setInternalAttenuation(double value)
{
    if (m_internalAttenuatorWidget) {
        m_internalAttenuatorWidget->setValue(value);
    }
}

static const char *kAttenuatorMime = "application/x-rfpm-attenuator";

void AttenuationManager::dragEnterEvent(QDragEnterEvent *event)
{
    if (!event->mimeData()->hasFormat(kAttenuatorMime)) {
        event->ignore();
        return;
    }
    AttenuatorWidget *src = qobject_cast<AttenuatorWidget*>(event->source());
    if (!src || !m_attenuatorWidgets.contains(src)) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
}

void AttenuationManager::dragMoveEvent(QDragMoveEvent *event)
{
    if (!event->mimeData()->hasFormat(kAttenuatorMime)) {
        event->ignore();
        return;
    }
    // Position arrives in AttenuationManager-local coords; the scroll
    // content lives one parent down. Map through global to stay correct
    // regardless of any future intermediate layout we might add.
    const QPoint posInContent = m_scrollContent->mapFromGlobal(mapToGlobal(event->position().toPoint()));
    const int gap = clampForInternal(dropIndexAt(posInContent));

    // Hide the indicator when the drop would be a no-op (back into the
    // source's own slot). Avoids the "ghost line clinging to the source"
    // feeling while the user is still deciding.
    AttenuatorWidget *src = qobject_cast<AttenuatorWidget*>(event->source());
    const int srcIdx = src ? m_attenuatorWidgets.indexOf(src) : -1;
    if (srcIdx >= 0 && (gap == srcIdx || gap == srcIdx + 1))
        hideInsertIndicator();
    else
        showInsertIndicatorAt(gap);
    event->acceptProposedAction();
}

void AttenuationManager::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
    hideInsertIndicator();
}

void AttenuationManager::dropEvent(QDropEvent *event)
{
    hideInsertIndicator();
    if (!event->mimeData()->hasFormat(kAttenuatorMime)) {
        event->ignore();
        return;
    }
    AttenuatorWidget *src = qobject_cast<AttenuatorWidget*>(event->source());
    if (!src) {
        event->ignore();
        return;
    }
    const int srcIdx = m_attenuatorWidgets.indexOf(src);
    if (srcIdx < 0) {
        event->ignore();
        return;
    }
    const QPoint posInContent = m_scrollContent->mapFromGlobal(mapToGlobal(event->position().toPoint()));
    const int dstGap = clampForInternal(dropIndexAt(posInContent));

    // No-op: dropping at the source's own slot or just past it.
    if (dstGap == srcIdx || dstGap == srcIdx + 1) {
        event->acceptProposedAction();
        return;
    }

    // Reorder m_attenuatorWidgets (canonical chain order). When the
    // destination gap was below the source's old position, removing the
    // source shifts everything below it up by one so the target index
    // drops by one too.
    m_attenuatorWidgets.removeAt(srcIdx);
    const int adjGap = (dstGap > srcIdx) ? dstGap - 1 : dstGap;
    m_attenuatorWidgets.insert(adjGap, src);

    // Mirror in m_listLayout. The indicator is no longer a layout child,
    // so layout indices match m_attenuatorWidgets indices directly.
    m_listLayout->removeWidget(src);
    m_listLayout->insertWidget(adjGap, src);

    event->acceptProposedAction();
    updateTotalAttenuation();
}

int AttenuationManager::dropIndexAt(const QPoint &posInScrollContent) const
{
    // Gap semantics: 0 = above first widget, k = between widgets k-1 and k,
    // N = below the last widget. Snap boundary is each widget's vertical
    // centre, which feels natural in practice.
    int gap = m_attenuatorWidgets.size();
    for (int i = 0; i < m_attenuatorWidgets.size(); ++i) {
        AttenuatorWidget *w = m_attenuatorWidgets.at(i);
        if (posInScrollContent.y() < w->geometry().center().y()) {
            gap = i;
            break;
        }
    }
    return gap;
}

int AttenuationManager::clampForInternal(int gapIndex) const
{
    if (!m_internalAttenuatorWidget) return gapIndex;
    const int internalIdx = m_attenuatorWidgets.indexOf(m_internalAttenuatorWidget);
    if (internalIdx < 0) return gapIndex;
    return std::min(gapIndex, internalIdx);
}

void AttenuationManager::showInsertIndicatorAt(int gapIndex)
{
    if (!m_insertIndicator) {
        m_insertIndicator = new QFrame(m_scrollContent);
        m_insertIndicator->setFrameShape(QFrame::HLine);
        m_insertIndicator->setLineWidth(2);
        m_insertIndicator->setStyleSheet("color: #0078d7; background: #0078d7;");
        m_insertIndicator->setFixedHeight(2);
    }
    // Position the indicator as a free-floating child rather than a layout
    // item; otherwise its insertion/removal reflows every plate below it
    // and we get a jitter loop near gap boundaries.
    int y = 0;
    if (m_attenuatorWidgets.isEmpty()) {
        y = 0;
    } else if (gapIndex < m_attenuatorWidgets.size()) {
        y = m_attenuatorWidgets.at(gapIndex)->geometry().top() - 2;
    } else {
        y = m_attenuatorWidgets.last()->geometry().bottom();
    }
    m_insertIndicator->setGeometry(0, y, m_scrollContent->width(), 2);
    m_insertIndicator->show();
    m_insertIndicator->raise();
}

void AttenuationManager::hideInsertIndicator()
{
    if (m_insertIndicator) m_insertIndicator->hide();
}
