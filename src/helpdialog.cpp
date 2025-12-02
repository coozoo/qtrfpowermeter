#include "helpdialog.h"
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QPushButton>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QDebug>
#include <QStringList>

HelpDialog::HelpDialog(const PMDeviceProperties &props, QWidget *parent)
    : QDialog(parent), m_properties(props)
{
    setWindowTitle(tr("Help: %1").arg(m_properties.name));
    setMinimumSize(600, 500);
    setAttribute(Qt::WA_DeleteOnClose);

    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setOpenExternalLinks(true);

    m_closeButton = new QPushButton(tr("Close"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &HelpDialog::accept);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_textBrowser);
    layout->addWidget(m_closeButton, 0, Qt::AlignRight);

    m_textBrowser->setHtml(buildHelpHtml());
}

HelpDialog::~HelpDialog() {}

QString HelpDialog::imageResourceToBase64(const QString &resourcePath)
{
    QImage image(resourcePath);
    if (image.isNull()) {
        qWarning() << "Could not load image from resource path:" << resourcePath;
        return QString();
    }
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return byteArray.toBase64();
}

QString HelpDialog::processHtmlImages(const QString &html)
{
    QString processedHtml = html;
    QRegularExpression re("<img[^>]*src=[\"'](qrc:([^\"']+))[\"'][^>]*>");
    auto it = re.globalMatch(html);

    while (it.hasNext()) {
        auto match = it.next();
        QString fullSrcAttribute = match.captured(1);
        QString resourcePath = match.captured(2);

        QString base64 = imageResourceToBase64(resourcePath);
        if (!base64.isEmpty()) {
            QString newSrc = "data:image/png;base64," + base64;
            processedHtml.replace(fullSrcAttribute, newSrc);
        }
    }
    return processedHtml;
}

QString HelpDialog::getDeviceSpecificInfo(const QString &deviceId)
{
    if (deviceId == "rf8000" || deviceId == "rf3000" || deviceId == "rf500") {
        return tr("<h2>Additional Information</h2>"
                  "<p>The group devices that communicates using a text-based protocol over serial.</p>"
                  "<p><b>Command Format:</b> <code>$FFFF+00.0#</code> where FFFF is the frequency in MHz and 00.0 is the offset in dB.</p>"
                  "<h3>Problems</h3>"
                  "<p>Device has very fragile protocol, there is two main problems with it and sure they persist in original software because actual problem is in firmware:</p>"
                  "<p><b>Broken character:</b><br>Device screen has updates, there is incoming data in log. <br>There is broken character on screen (see image below) and as well in protocol, so the data is incorrect and cannot be parsed.</p>"
                  "<br><img src=\"qrc:/help/img/rf8000brokenchar.png\" alt=\"rf8000 broken charachters\" width=\"300\">"
                  "<p><b>Hanging:</b><br>Device screen is frozen, no incoming data. <br>Device can hang if send commands to often or some unknown reason.</p>"
                  "<h3>Solutions</h3>"
                  "<p><b>Broken character:</b><br>Simply press Set device parameters in app, like frequency, and it will be fixed.</p>"
                  "<p><b>Hanging:</b><br><p>Connect device in safe proof order"
                  "<ul><li>Connect/restart device (device in wait mode. It shows logo on device screen)</li>"
                  "<li>Refresh devices list in app (select correct device type)</li>"
                  "<li>Press connect in app</li>"
                  "<li>On device press middle button to start capture</li></ul>"
                  "<br>Other sequences could work but sometimes glitches may appear.</p>");
    }
    if (deviceId == "rfpm_v7_10ghz") {
        return tr("<h2>Additional Information</h2>"
                  "<p>The V7 device uses a complex command set, including commands to start and stop data streams.</p>"
                  "<p><b>Example Command:</b> <code>IC000+W+00.0+00.00+1000</code></p>");
    }
    if (deviceId == "rfpmv5") {
        return tr("<h2>Device Description</h2>"
                  "<p>Device similar to the rf8000 but with a more stable and reliable protocol. It can send up to 3000 updates per second, so the application uses an averaging algorithm to prevent UI overload.</p>"
                  "<h2>Additional Information</h2>"
                  "<p>The device uses a modified text-based protocol for communication.</p>"
                  "<p><b>Command Format:</b> <code>AFFFFS00.00</code> where A is a literal character, FFFF is the frequency in MHz, S is the sign character (+ or -), and 00.00 is the offset in dB.</p>");
    }
    return tr("<h2>Additional Information</h2><p>No device-specific information available.</p>");
}

QString HelpDialog::buildHelpHtml()
{
    const QString templatePath = ":/help/device_help_template.html";
    QFile templateFile(templatePath);
    if (!templateFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return tr("<h2>Error</h2><p>Could not load help template.</p>");
    }
    QString html = templateFile.readAll();

    // --- Populate all placeholders with translatable strings and data ---

    // Titles
    html.replace("%SPECS_TITLE%", tr("Device Specifications"));
    html.replace("%PARAM_TITLE%", tr("Parameter"));
    html.replace("%VALUE_TITLE%", tr("Value"));
    html.replace("%FREQ_RANGE_TITLE%", tr("Frequency Range"));
    html.replace("%POWER_RANGE_TITLE%", tr("Power Range"));
    html.replace("%BAUD_RATE_TITLE%", tr("Baud Rate"));
    html.replace("%INTERNAL_ATT_TITLE%", tr("Internal Attenuator"));
    html.replace("%VID_PID_TITLE%", tr("VID:PID"));

    // Dynamic Values
    html.replace("%DEVICE_NAME%", m_properties.name);
    html.replace("%ALTERNATIVE_NAMES%", m_properties.alternativeNames);
    html.replace("%FREQ_RANGE_VALUE%", tr("%1 MHz to %2 MHz").arg(m_properties.minFreqHz / 1e6).arg(m_properties.maxFreqHz / 1e6));
    html.replace("%POWER_RANGE_VALUE%", tr("%1 dBm to %2 dBm").arg(m_properties.minPowerDbm).arg(m_properties.maxPowerDbm));
    html.replace("%BAUD_RATE_VALUE%", QString::number(m_properties.baudRate));
    html.replace("%INTERNAL_ATT_VALUE%", m_properties.hasInternalAttenuator ? tr("Yes") : tr("No"));

    // Format VID:PID pairs
    QStringList vidPidList;
    for (const auto &pair : m_properties.supportedVidPids) {
        vidPidList << QString("0x%1:0x%2").arg(pair.first, 4, 16, QChar('0')).arg(pair.second, 4, 16, QChar('0'));
    }
    html.replace("%VID_PID_VALUE%", vidPidList.join(", "));


    // Image Path placeholder
    html.replace("%DEVICE_IMAGE_PATH%", m_properties.imagePath);

    // Get and insert the device-specific content block
    QString additionalInfo = getDeviceSpecificInfo(m_properties.id);
    html.replace("%ADDITIONAL_INFO_CONTENT%", additionalInfo);

    // composed HTML to convert all qrc: images to Base64
    return processHtmlImages(html);
}
