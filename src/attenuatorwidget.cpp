#include "attenuatorwidget.h"
#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QPixmap>
#include <cmath>
#include <limits>

AttenuatorWidget::AttenuatorWidget(AttenuatorWidget::AttenuatorType type, QWidget *parent)
    : QGroupBox(parent), m_type(type), m_attenuationValue(0.0), m_effectiveValue(0.0),
    m_markedForRemoval(false),
    m_editorHasBeenShown(false), // Initialize the flag to false
    m_digitalControl(nullptr), m_fixedControl(nullptr),
    m_internalControl(nullptr), m_cableManager(nullptr)
{
    setupUi();
    installEventFilter(this);

    if (m_type == Digital)
    {
        m_digitalControl = new QtDigitalAttenuator();
        // Digital widget uses effectiveAttenuationChanged as the canonical
        // source: it carries nominal (for the LCD) and effective (for the
        // chain total) together so they cannot drift apart.
        connect(m_digitalControl, &QtDigitalAttenuator::effectiveAttenuationChanged,
                this, &AttenuatorWidget::onEffectiveChanged);
        connect(m_digitalControl, &QtDigitalAttenuator::valueSetStatus, this, &AttenuatorWidget::onStatusChanged);
        connect(m_digitalControl, &QtDigitalAttenuator::modelChanged, this, &AttenuatorWidget::onDescriptionChanged);
        connect(m_digitalControl, &QtDigitalAttenuator::maxInputDbmChanged, this,
                [this](double v, const QString &) { emit maxInputDbmChanged(v); });
        onDescriptionChanged(tr("Digital"));
    }
    else if (m_type == Fixed)
    {
        m_fixedControl = new FixedAttenuatorControl();
        connect(m_fixedControl, &FixedAttenuatorControl::valueChanged, this, &AttenuatorWidget::onValueChanged);
        connect(m_fixedControl, &FixedAttenuatorControl::descriptionChanged, this, &AttenuatorWidget::onDescriptionChanged);
        onDescriptionChanged(tr("Fixed"));
    }
    else if (m_type == InternalDigital)
    {
        m_internalControl = new InternalAttenuatorControl();
        connect(m_internalControl, &InternalAttenuatorControl::valueChanged, this, &AttenuatorWidget::onValueChanged);
        connect(m_internalControl, &InternalAttenuatorControl::descriptionChanged, this, &AttenuatorWidget::onDescriptionChanged);
        onDescriptionChanged(tr("Device Built-in"));
    }
    else if (m_type == Cable)
    {
        m_cableManager = new QtCoaxCableLossCalcManager();
        m_cableManager->setWindowTitle(tr("Coaxial Cable Loss Calculator"));
        m_cableManager->loadCablesFromJson(":/cables.json");
        m_cableManager->resize(800, 600);
        m_cableManager->setMinimumHeight(500);
        m_cableManager->setIndividualLengthAllowed(true);
        m_cableManager->setAllowCableDupes(true);
        connect(m_cableManager, &QtCoaxCableLossCalcManager::totalAttenuationChanged, this, &AttenuatorWidget::onValueChanged);
        onDescriptionChanged(tr("Cable Loss"));
    }
}

AttenuatorWidget::~AttenuatorWidget()
{
    qDebug()<<"AttenuatorWidget destructor called.";
    if (m_digitalControl) delete m_digitalControl;
    if (m_fixedControl) delete m_fixedControl;
    if (m_internalControl) delete m_internalControl;
    if (m_cableManager) delete m_cableManager;
}

void AttenuatorWidget::setupUi()
{
    setTitle("");
    setCursor(Qt::PointingHandCursor);

    QHBoxLayout *layout = new QHBoxLayout(this);

    QCheckBox *removeCheckBox = new QCheckBox(this);
    removeCheckBox->setCursor(Qt::ArrowCursor);
    connect(removeCheckBox, &QCheckBox::toggled, this, &AttenuatorWidget::onCheckBoxToggled);

    m_typeLabel = new QLabel(this);
    QPixmap pixmapFromFile;
    QString tooltip;

    if (m_type == Fixed) {
        pixmapFromFile = QPixmap(":/images/AttenuatorRF.svg");
        tooltip = tr("Fixed Attenuator");
    } else if (m_type == Digital) {
        pixmapFromFile = QPixmap(":/images/digiattcontrol.svg");
        tooltip = tr("Digital Attenuator");
    } else if (m_type == InternalDigital) {
        pixmapFromFile = QPixmap(":/images/devices/rf8000.png");
        tooltip = tr("Device Internal Attenuator");
        removeCheckBox->setDisabled(true);
        removeCheckBox->setToolTip(tr("The internal attenuator is part of the device and cannot be removed."));
    } else if (m_type == Cable) {
        pixmapFromFile = QPixmap(":/images/coaxcable.svg");
        tooltip = tr("Coaxial Cable Loss");
    }

    m_typeLabel->setPixmap(pixmapFromFile.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_typeLabel->setToolTip(tooltip);

    m_valueLcd = new QLCDNumber(this);
    m_valueLcd->setSegmentStyle(QLCDNumber::Flat);
    m_valueLcd->setDigitCount(5);
    m_valueLcd->display(m_attenuationValue);
    m_valueLcd->setStyleSheet("QLCDNumber{ background-color: green; color: yellow;}");

    m_descrLabel = new QLabel(this);
    m_descrLabel->setStyleSheet("background-color: #E6E6FA; color: #333333; border: 1px solid gray; border-radius: 5px; padding: 4px;");

    layout->addWidget(removeCheckBox);
    layout->addWidget(m_typeLabel);
    layout->addWidget(m_valueLcd);
    layout->addWidget(new QLabel("dB", this));
    layout->addWidget(m_descrLabel);
    layout->addStretch();

    setLayout(layout);
    setPressedStyle(false);
    setFixedHeight(60);
}

void AttenuatorWidget::onStatusChanged(bool status)
{
    if (status) {
        m_valueLcd->setStyleSheet("QLCDNumber{ background-color: green; color: yellow;}");
    } else {
        m_valueLcd->setStyleSheet("QLCDNumber{ background-color: red; color: yellow;}");
    }
}

double AttenuatorWidget::getAttenuation() const
{
    return m_effectiveValue;
}

double AttenuatorWidget::maxInputDbm() const
{
    if (m_digitalControl) return m_digitalControl->maxInputDbm();
    // Fixed: user-settable rating arrives in step 6g; cables and the
    // device-internal stage stay NaN here (the meter's own max-dbm window
    // covers the internal case from a different path in the calculator).
    return std::numeric_limits<double>::quiet_NaN();
}

void AttenuatorWidget::onCheckBoxToggled(bool checked)
{
    m_markedForRemoval = checked;
}

void AttenuatorWidget::setValue(double value)
{
    onValueChanged(value);
}

void AttenuatorWidget::onValueChanged(double value)
{
    qDebug()<<Q_FUNC_INFO<<value;
    // Non-digital path: nominal and effective are the same number. Digital
    // updates take onEffectiveChanged so the LCD stays on nominal while the
    // chain total picks up nominal + IL.
    m_attenuationValue = value;
    m_effectiveValue = value;
    m_valueLcd->display(m_attenuationValue);
    emit valueChanged(m_effectiveValue);
}

void AttenuatorWidget::onEffectiveChanged(double nominalDb, double ilDb, double effectiveDb)
{
    Q_UNUSED(ilDb);
    qDebug()<<Q_FUNC_INFO<<nominalDb<<ilDb<<effectiveDb;
    m_attenuationValue = nominalDb;
    m_effectiveValue = effectiveDb;
    m_valueLcd->display(m_attenuationValue);
    emit valueChanged(m_effectiveValue);
}

void AttenuatorWidget::setCurrentFrequencyHz(double freqHz)
{
    if (m_digitalControl) m_digitalControl->setCurrentFrequencyHz(freqHz);
}

void AttenuatorWidget::openEditor()
{
    QWidget *editor = nullptr;

    if (m_digitalControl)
    {
        editor = m_digitalControl;
    }
    else if (m_fixedControl)
    {
        m_fixedControl->setValue(m_attenuationValue);
        m_fixedControl->setDescription(m_description);
        editor = m_fixedControl;
    }
    else if (m_internalControl)
    {
        m_internalControl->setValue(m_attenuationValue);
        m_internalControl->setDescription(m_description);
        editor = m_internalControl;
    }
    else if (m_cableManager)
    {
        editor = m_cableManager;
    }

    if (editor)
    {
        // --- This is the new logic ---
        // If the editor has never been shown before, position it.
        if (!m_editorHasBeenShown)
        {
            QWidget *mainWindow = this->window();
            if (mainWindow) {
                // Center it on the main window
                QPoint centerPos = mainWindow->geometry().center() - editor->rect().center();
                editor->move(centerPos);
            }
            m_editorHasBeenShown = true; // Set flag so we don't do this again
        }

        editor->show();
        editor->raise();
        editor->activateWindow();
        editor->adjustSize();
    }
}

void AttenuatorWidget::setPressedStyle(bool pressed)
{
    if (pressed) {
        setStyleSheet("QGroupBox { border: 2px solid #0078d7; background-color: #e0e0e0; border-radius: 4px; }");
    } else {
        setStyleSheet("QGroupBox { border: 1px solid #adadad; background-color: #f0f0f0; border-radius: 4px; }");
    }
}

bool AttenuatorWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != this)
        return QObject::eventFilter(watched, event);

    auto *mev = static_cast<QMouseEvent *>(event);

    if (event->type() == QEvent::MouseButtonPress) {
        QWidget *child = childAt(mev->pos());
        if (qobject_cast<QCheckBox *>(child)) { return false; }
        // Internal sub-control is pinned (it represents the device's own
        // front-end). Press still triggers the open-editor path on release,
        // but never starts a drag.
        m_dragStartPos = (m_type == InternalDigital) ? QPoint(-1, -1) : mev->pos();
        setPressedStyle(true);
        return true;
    }

    if (event->type() == QEvent::MouseMove) {
        if (m_dragStartPos.x() < 0) return false;
        if (!(mev->buttons() & Qt::LeftButton)) return false;
        if ((mev->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance())
            return false;
        // Cross the threshold: start a drag. The release that closes the
        // drag must not also fire openEditor(), so clear m_dragStartPos
        // before exec() and rely on the negative sentinel in release below.
        m_dragStartPos = QPoint(-1, -1);
        setPressedStyle(false);

        QDrag *drag = new QDrag(this);
        QMimeData *mime = new QMimeData();
        // Payload is irrelevant; AttenuationManager identifies the source
        // via QDrag::source(). The custom mime type is the filter that
        // distinguishes our drag from foreign QDrag flows.
        mime->setData(QStringLiteral("application/x-rfpm-attenuator"), QByteArray());
        drag->setMimeData(mime);
        QPixmap shot = grab();
        drag->setPixmap(shot);
        drag->setHotSpot(mev->pos());
        drag->exec(Qt::MoveAction);
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        QWidget *child = childAt(mev->pos());
        if (qobject_cast<QCheckBox *>(child)) { setPressedStyle(false); m_dragStartPos = QPoint(-1, -1); return false; }
        setPressedStyle(false);
        // If a drag fired, m_dragStartPos was reset above and we suppress
        // the click-to-open-editor path here.
        const bool wasPressTracked = (m_dragStartPos.x() >= 0);
        m_dragStartPos = QPoint(-1, -1);
        if (wasPressTracked && rect().contains(mev->pos())) {
            openEditor();
        }
        return true;
    }

    return QObject::eventFilter(watched, event);
}

void AttenuatorWidget::onDescriptionChanged(const QString &description)
{
    m_description = description;
    m_descrLabel->setText(description);
    if (m_digitalControl) {
        m_digitalControl->setWindowTitle(tr("Digital Attenuator: ") + description);
    }
}

void AttenuatorWidget::setInternalProperties(double min, double max, double step)
{
    if (m_internalControl) {
        m_internalControl->setProperties(min, max, step);
    }
}
