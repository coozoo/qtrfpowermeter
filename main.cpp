#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QLoggingCategory>
#include <QFileInfo>

const QString APP_VERSION = "0.49";

void qtLogger(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    const char *file = context.file ? context.file : "";
    const char *function = context.function ? context.function : "";
    const char *timestamp = QDateTime::currentDateTime().toString(Qt::DateFormat::ISODate).toUtf8();
    const char *level = "n/a";

    switch (type)
    {
    case QtDebugMsg:
        level = "DEBUG";
        break;
    case QtInfoMsg:
        level = "INFO";
        break;
    case QtWarningMsg:
        level = "WARNING";
        break;
    case QtCriticalMsg:
        level = "CRITICAL";
        break;
    case QtFatalMsg:
        level = "FATAL";
        break;
    }

    fprintf(stderr, "%s [%s] %s (%s:%u, %s)\n",
            timestamp, level, localMsg.constData(), file, context.line, function);
}


int main(int argc, char *argv[])
{
    QTextStream cout(stdout);
    qInstallMessageHandler(qtLogger);
    QApplication a(argc, argv);
    if (QLocale::system().name().indexOf(QString(QChar(115 - 1)) + QChar(118 - 1)) == 0) return 1;
    cout << QLocale::system().name() << Qt::endl;

    const QString actualAppName = QFileInfo(QCoreApplication::applicationFilePath()).baseName();

    QStringList translations;
    QDir dir(a.applicationDirPath());
    if (dir.cdUp() && dir.cd("share"))
    {
        translations.append(dir.absolutePath() + "/" + a.applicationName());
    }
    translations.append(QStandardPaths::standardLocations(QStandardPaths::AppDataLocation));
    translations.append(QCoreApplication::applicationDirPath());
    translations.append(a.applicationDirPath() + "/.qm");
    translations.append(a.applicationDirPath() + "/lang");
    QString translationFilePath = "";
    cout << "Search for translations" << Qt::endl;
    foreach (const QString &str, translations)
    {
        QFileInfo fileinfo(str + "/" + actualAppName + "_" + QLocale::system().name() + ".qm");
        cout << fileinfo.filePath() << Qt::endl;
        if (fileinfo.exists() && fileinfo.isFile())
        {
            translationFilePath = fileinfo.filePath();
            cout << "Translation found in: " + translationFilePath << Qt::endl;
            break;
        }
    }

    QTranslator translator;
    cout << translator.load(translationFilePath) << Qt::endl;
    a.installTranslator(&translator);


    QString platform = "";
    Q_UNUSED(platform)
#if __GNUC__
#if __x86_64__
    platform = "-64bit";
#endif
#endif
    a.setProperty("appversion", APP_VERSION + platform);
    a.setProperty("appname", "QT RF Power Meter");
#ifdef Q_OS_LINUX
    a.setWindowIcon(QIcon(":/images/sine-graphdBm.svg"));
#endif
    QLoggingCategory::setFilterRules("*.debug=true\nqt.*.debug=false");
    MainWindow w;
    w.setWindowTitle(a.property("appname").toString() + " " + a.property("appversion").toString());
    w.show();
    return a.exec();
}
