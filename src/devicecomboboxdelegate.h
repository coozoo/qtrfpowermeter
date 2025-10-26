#ifndef DEVICECOMBOBOXDELEGATE_H
#define DEVICECOMBOBOXDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QApplication>
#include <QStyle>

class DeviceComboBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit DeviceComboBoxDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

#endif // DEVICECOMBOBOXDELEGATE_H
