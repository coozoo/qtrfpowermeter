#include "advancedcalibrationtablemodel.h"
#include "calibrationmodel.h"
#include <QBrush>
#include <QColor>

AdvancedCalibrationTableModel::AdvancedCalibrationTableModel(CalibrationModel *source,
                                                             QObject *parent)
    : QAbstractTableModel(parent)
    , m_source(source)
{
    if (m_source) {
        connect(m_source, &CalibrationModel::advancedTableChanged,
                this, &AdvancedCalibrationTableModel::onSourceChanged);
        connect(m_source, &QAbstractItemModel::modelReset,
                this, &AdvancedCalibrationTableModel::onSourceChanged);
    }
}

int AdvancedCalibrationTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_source) return 0;
    return m_source->advancedRows().size();
}

int AdvancedCalibrationTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_source) return 0;
    return m_source->advancedPowerAxis().size();
}

QVariant AdvancedCalibrationTableModel::data(const QModelIndex &index, int role) const
{
    if (!m_source || !index.isValid()) return {};
    const auto &rows = m_source->advancedRows();
    if (index.row() < 0 || index.row() >= rows.size()) return {};
    const auto &cells = rows[index.row()].cells;
    if (index.column() < 0 || index.column() >= cells.size()) return {};
    const auto &cell = cells[index.column()];
    if (role == Qt::DisplayRole) {
        return cell.isSet ? QString::number(cell.correctionDb, 'f', 3)
                          : QStringLiteral("-");
    }
    if (role == Qt::EditRole) {
        return cell.isSet ? QVariant(cell.correctionDb) : QVariant();
    }
    if (role == Qt::TextAlignmentRole) {
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }
    if (role == Qt::BackgroundRole && cell.isSet) {
        // Soft green for filled cells so the user can see at a glance
        // which (freq, level) pairs have been calibrated.
        return QBrush(QColor(180, 230, 180));
    }
    return {};
}

bool AdvancedCalibrationTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!m_source || !index.isValid() || role != Qt::EditRole) return false;
    const auto &rows = m_source->advancedRows();
    if (index.row() < 0 || index.row() >= rows.size()) return false;
    if (index.column() < 0 || index.column() >= m_source->advancedPowerAxis().size()) return false;

    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        m_source->clearAdvancedCell(index.row(), index.column());
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }
    bool ok = false;
    const double v = text.toDouble(&ok);
    if (!ok) return false;
    if (!m_source->setAdvancedCell(index.row(), index.column(), v)) return false;
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

QVariant AdvancedCalibrationTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (!m_source) return {};
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Horizontal) {
        const auto &axis = m_source->advancedPowerAxis();
        if (section < 0 || section >= axis.size()) return {};
        return QString::number(axis[section], 'f', 1) + " dBm";
    }
    const auto &rows = m_source->advancedRows();
    if (section < 0 || section >= rows.size()) return {};
    const double f = rows[section].frequencyMHz;
    if (f >= 1000.0) {
        return QString::number(f / 1000.0, 'f', 3) + " GHz";
    }
    return QString::number(f, 'f', 3) + " MHz";
}

Qt::ItemFlags AdvancedCalibrationTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

void AdvancedCalibrationTableModel::onSourceChanged()
{
    beginResetModel();
    endResetModel();
}
