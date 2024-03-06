#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QLoggingCategory>

const QString APP_VERSION = "0.2b";

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
    qInstallMessageHandler(qtLogger);

    QApplication a(argc, argv);
    QString platform = "";
    Q_UNUSED(platform)
#if __GNUC__
#if __x86_64__
    platform = "-64bit";
#endif
#endif
    a.setProperty("appversion", APP_VERSION + platform);
    a.setProperty("appname", "QT RF Power Meter");
    QLoggingCategory::setFilterRules("*.debug=true\nqt.*.debug=false");
    MainWindow w;
    w.setWindowTitle(a.property("appname").toString() + " " + a.property("appversion").toString());
    w.show();
    return a.exec();
}
