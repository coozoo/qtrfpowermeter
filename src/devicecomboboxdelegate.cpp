#include "devicecomboboxdelegate.h"
#include <QFontMetrics>

DeviceComboBoxDelegate::DeviceComboBoxDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void DeviceComboBoxDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    QString name = index.data(Qt::DisplayRole).toString();
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

    QRect r = option.rect;
    int iconSize = 50;
    int padding = 5;
    QRect iconRect = QRect(r.left() + padding, r.top() + (r.height() - iconSize) / 2, iconSize, iconSize);
    QRect textRect = QRect(iconRect.right() + padding, r.top(), r.width() - iconRect.width() - padding * 3, r.height());

    icon.paint(painter, iconRect);

    QColor textColor = (option.state & QStyle::State_Selected) ?
                           option.palette.color(QPalette::HighlightedText) :
                           option.palette.color(QPalette::Text);
    painter->setPen(textColor);

    QFontMetrics fontMetrics(painter->font());
    QString elidedName = fontMetrics.elidedText(name, Qt::ElideRight, textRect.width());
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);

    painter->restore();
}

QSize DeviceComboBoxDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{

    QString name = index.data(Qt::DisplayRole).toString();

    int iconSize = 50;
    int padding = 5;


    QFontMetrics fontMetrics(option.font);
    int textWidth = fontMetrics.horizontalAdvance(name);


    int totalWidth = padding + iconSize + padding + textWidth + padding;


    return QSize(totalWidth, 70);
}
