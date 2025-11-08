#ifndef HELPDIALOG_H
#define HELPDIALOG_H

#include <QDialog>
#include "pmdeviceproperties.h"

class QTextBrowser;
class QPushButton;

class HelpDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HelpDialog(const PMDeviceProperties &props, QWidget *parent = nullptr);
    ~HelpDialog();

private:
    QString buildHelpHtml();
    QString getDeviceSpecificInfo(const QString &deviceId);
    QString processHtmlImages(const QString &html);
    QString imageResourceToBase64(const QString &resourcePath);

    QTextBrowser *m_textBrowser;
    QPushButton *m_closeButton;
    const PMDeviceProperties m_properties;
};

#endif // HELPDIALOG_H
