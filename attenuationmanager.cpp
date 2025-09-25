#include "attenuationmanager.h"
#include "attenuatorwidget.h"



AttenuationManager::AttenuationManager(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

AttenuationManager::~AttenuationManager()
{
    qDeleteAll(m_attenuatorWidgets);
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

    m_typeComboBox->addItem(fixedIcon, tr("Fixed Attenuator"), AttenuatorWidget::Fixed);
    m_typeComboBox->addItem(digitalIcon, tr("Digital Attenuator"), AttenuatorWidget::Digital);

    QPushButton *addButton = new QPushButton(tr("Add"));
    addButton->setToolTip(tr("Add Attenuator"));
    QPushButton *removeButton = new QPushButton(tr("Remove"));
    removeButton->setToolTip(tr("Remove Selected"));

    controlLayout->addWidget(m_typeComboBox);
    controlLayout->addWidget(addButton);
    controlLayout->addWidget(removeButton);
    controlLayout->addStretch();

    // --- Scroll Area for the list of widgets ---
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *scrollContent = new QWidget();
    m_listLayout = new QVBoxLayout(scrollContent);
    m_listLayout->addStretch();

    scrollArea->setWidget(scrollContent);

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

    mainLayout->addLayout(controlLayout);
    mainLayout->addWidget(scrollArea);
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

    // Add to our list of pointers
    m_attenuatorWidgets.append(newWidget);

    // Add to the visual layout (insert before the stretch)
    m_listLayout->insertWidget(m_listLayout->count() - 1, newWidget);

    qDebug()<<"Added new attenuator. Total count:"<<m_attenuatorWidgets.count();

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
}
