#include "attenuatorwidget.h"

AttenuatorWidget::AttenuatorWidget(AttenuatorWidget::AttenuatorType type, QWidget *parent)
    : QGroupBox(parent), m_type(type), m_attenuationValue(0.0), m_markedForRemoval(false),
    m_editorHasBeenShown(false), // Initialize the flag to false
    m_digitalControl(nullptr), m_fixedControl(nullptr)
{
    setupUi();
    installEventFilter(this);

    if (m_type == Digital)
    {
        m_digitalControl = new QtDigitalAttenuator();
        connect(m_digitalControl, &QtDigitalAttenuator::currentValueChanged, this, &AttenuatorWidget::onValueChanged);
        connect(m_digitalControl, &QtDigitalAttenuator::valueSetStatus, this, &AttenuatorWidget::onStatusChanged);
        connect(m_digitalControl, &QtDigitalAttenuator::modelChanged, this, &AttenuatorWidget::onDescriptionChanged);
        onDescriptionChanged(tr("Digital"));
    }
    else if (m_type == Fixed)
    {
        m_fixedControl = new FixedAttenuatorControl();
        connect(m_fixedControl, &FixedAttenuatorControl::valueChanged, this, &AttenuatorWidget::onValueChanged);
        connect(m_fixedControl, &FixedAttenuatorControl::descriptionChanged, this, &AttenuatorWidget::onDescriptionChanged);
        onDescriptionChanged(tr("Fixed"));
    }
}

AttenuatorWidget::~AttenuatorWidget()
{
    qDebug()<<"AttenuatorWidget destructor called.";
    if (m_digitalControl) delete m_digitalControl;
    if (m_fixedControl) delete m_fixedControl;
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
    QPixmap pixmapFromFile = (m_type == Fixed) ? QPixmap(":/images/AttenuatorRF.svg") : QPixmap(":/images/digiattcontrol.svg");
    m_typeLabel->setPixmap(pixmapFromFile.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_typeLabel->setToolTip((m_type == Fixed) ? tr("Fixed") : tr("Digital"));

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
    return m_attenuationValue;
}

void AttenuatorWidget::onCheckBoxToggled(bool checked)
{
    m_markedForRemoval = checked;
}

void AttenuatorWidget::onValueChanged(double value)
{
    qDebug()<<Q_FUNC_INFO<<value;
    m_attenuationValue = value;
    m_valueLcd->display(m_attenuationValue);
    emit valueChanged(m_attenuationValue);
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
    if (watched == this) {
        if (event->type() == QEvent::MouseButtonPress) {
            QWidget *child = childAt(static_cast<QMouseEvent *>(event)->pos());
            if (qobject_cast<QCheckBox *>(child)) { return false; }
            setPressedStyle(true);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            QWidget *child = childAt(static_cast<QMouseEvent *>(event)->pos());
            if (qobject_cast<QCheckBox *>(child)) { setPressedStyle(false); return false; }
            setPressedStyle(false);
            if (rect().contains(static_cast<QMouseEvent *>(event)->pos())) {
                openEditor();
            }
            return true;
        }
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
