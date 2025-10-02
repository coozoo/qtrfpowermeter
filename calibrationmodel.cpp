#include "calibrationmodel.h"
#include <QColor>

CalibrationModel::CalibrationModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int CalibrationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_points.count();
}

int CalibrationModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant CalibrationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_points.count())
        return QVariant();

    const CalibrationPoint &point = m_points[index.row()];

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case Frequency:
            return QString::number(point.frequencyMHz, 'f', 3);
        case Correction:
            return point.isSet ? QString::number(point.correctionDb, 'f', 2) : QVariant();
        }
    }

    if (role == Qt::BackgroundRole) {
        if (point.isSet) {
            return QColor(220, 255, 220); // A light green for set values
        }
    }

    return QVariant();
}

bool CalibrationModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole || !index.isValid() || index.row() >= m_points.count())
        return false;

    CalibrationPoint &point = m_points[index.row()];
    bool ok;

    if (index.column() == Correction) {
        double correction = value.toDouble(&ok);
        if (ok) {
            point.correctionDb = correction;
            point.isSet = true;
            emit dataChanged(index, index, {Qt::DisplayRole, Qt::BackgroundRole});
            return true;
        }
    }

    return false;
}

QVariant CalibrationModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
    case Frequency:
        return tr("Frequency (MHz)");
    case Correction:
        return tr("Correction (dB)");
    }
    return QVariant();
}

Qt::ItemFlags CalibrationModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

void CalibrationModel::generateFrequencies(double startMHz, double endMHz, double stepMHz)
{
    beginResetModel();
    m_points.clear();
    if (stepMHz <= 0 || startMHz > endMHz) {
        endResetModel();
        return;
    }

    for (double freq = startMHz; freq <= endMHz; freq += stepMHz) {
        m_points.append({freq, 0.0, false});
    }
    // Ensure the last point is exactly the end frequency
    if (m_points.isEmpty() || m_points.last().frequencyMHz < endMHz) {
        m_points.append({endMHz, 0.0, false});
    }

    endResetModel();
}

void CalibrationModel::clear()
{
    beginResetModel();
    m_points.clear();
    endResetModel();
}

const QVector<CalibrationPoint>& CalibrationModel::getPoints() const
{
    return m_points;
}

void CalibrationModel::setPoints(const QVector<CalibrationPoint> &points)
{
    beginResetModel();
    m_points = points;
    endResetModel();
}

double CalibrationModel::getCorrection(double frequencyMHz) const
{
    if (m_points.isEmpty()) {
        return 0.0;
    }

    // Find two adjacent points to interpolate between
    const CalibrationPoint *p1 = nullptr;
    const CalibrationPoint *p2 = nullptr;

    for (const auto &point : m_points) {
        if (point.isSet && point.frequencyMHz <= frequencyMHz) {
            p1 = &point;
        }
        if (point.isSet && point.frequencyMHz >= frequencyMHz && !p2) {
            p2 = &point;
            break;
        }
    }

    if (!p1 && !p2) return 0.0; // No set points
    if (p1 && !p2) return p1->correctionDb; // Extrapolate from last point
    if (!p1 && p2) return p2->correctionDb; // Extrapolate from first point
    if (p1 == p2) return p1->correctionDb; // Exact match

    // Linear interpolation
    double x1 = p1->frequencyMHz;
    double y1 = p1->correctionDb;
    double x2 = p2->frequencyMHz;
    double y2 = p2->correctionDb;

    return y1 + (y2 - y1) * (frequencyMHz - x1) / (x2 - x1);
}
