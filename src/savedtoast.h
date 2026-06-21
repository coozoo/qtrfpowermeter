#ifndef SAVEDTOAST_H
#define SAVEDTOAST_H

#include <QString>

class QWidget;

namespace notify {

// Show a small, frameless, auto-dismissing notification near `anchor`
// reporting that something was saved. Replaces blocking QMessageBox::exec()
// calls in the save-image / save-csv code paths.
//
// The toast disappears after `holdMs` and gets a brief fade-out. If
// `clickToOpen` is non-empty, clicking the toast opens that path/url
// via QDesktopServices (useful for "click to open folder").
void showSavedToast(QWidget *anchor,
                    const QString &message,
                    const QString &clickToOpen = QString(),
                    int holdMs = 2200);

} // namespace notify

#endif // SAVEDTOAST_H
