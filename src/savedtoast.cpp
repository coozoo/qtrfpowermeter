#include "savedtoast.h"

#include <QLabel>
#include <QWidget>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QMouseEvent>

namespace {

class Toast : public QLabel
{
public:
    Toast(QWidget *parent, const QString &message, const QString &clickToOpen)
        : QLabel(message, parent ? parent->window() : nullptr,
                 Qt::ToolTip | Qt::FramelessWindowHint),
          m_clickToOpen(clickToOpen)
    {
        setAttribute(Qt::WA_DeleteOnClose);
        setAttribute(Qt::WA_TransparentForMouseEvents, false);
        setStyleSheet(QStringLiteral(
            "QLabel {"
            "  background: rgba(40,40,40,220);"
            "  color: white;"
            "  padding: 8px 14px;"
            "  border-radius: 6px;"
            "  font-size: 11pt;"
            "}"));
        setTextFormat(Qt::PlainText);
        setCursor(clickToOpen.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
        adjustSize();
    }

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && !m_clickToOpen.isEmpty()) {
            const QFileInfo fi(m_clickToOpen);
            const QString opener = fi.exists() ? fi.absolutePath() : m_clickToOpen;
            QDesktopServices::openUrl(QUrl::fromLocalFile(opener));
        }
        close();
    }

private:
    QString m_clickToOpen;
};

} // namespace

namespace notify {

void showSavedToast(QWidget *anchor,
                    const QString &message,
                    const QString &clickToOpen,
                    int holdMs)
{
    QWidget *window = anchor ? anchor->window() : nullptr;
    Toast *toast = new Toast(anchor, message, clickToOpen);

    // Position near the bottom-centre of the anchor's window. Bottom is
    // less likely to occlude the chart / controls the user just used.
    if (window) {
        const QPoint windowBottomCentre = window->mapToGlobal(
            QPoint(window->width() / 2, window->height() - 24));
        toast->move(windowBottomCentre.x() - toast->width() / 2,
                    windowBottomCentre.y() - toast->height());
    }

    // Soft fade-in so the toast doesn't startle the eye.
    auto *fx = new QGraphicsOpacityEffect(toast);
    fx->setOpacity(0.0);
    toast->setGraphicsEffect(fx);
    toast->show();

    auto *fadeIn = new QPropertyAnimation(fx, "opacity", toast);
    fadeIn->setDuration(120);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    QTimer::singleShot(holdMs, toast, [toast, fx]() {
        if (!toast) return;
        auto *fadeOut = new QPropertyAnimation(fx, "opacity", toast);
        fadeOut->setDuration(220);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        QObject::connect(fadeOut, &QPropertyAnimation::finished, toast, &QWidget::close);
        fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

} // namespace notify
