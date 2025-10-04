#include "calibrationmodel.h"
#include <QColor>
#include <QtMath>

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

    if (role == Qt::DisplayRole || role == Qt::EditRole)
        {
            switch (index.column())
                {
                case Frequency:
                    return QString::number(point.frequencyMHz, 'f', 3);
                case Correction:
                    return point.isSet ? QString::number(point.correctionDb, 'f', 2) : QVariant();
                }
        }

    if (role == Qt::BackgroundRole)
        {
            if (point.isSet)
                {
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

    if (index.column() == Correction)
        {
            double correction = value.toDouble(&ok);
            if (ok)
                {
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

    switch (section)
        {
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

    Qt::ItemFlags defaultFlags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    if (index.column() == Correction)
        {
            return defaultFlags | Qt::ItemIsEditable;
        }

    return defaultFlags;
}

void CalibrationModel::generateFrequencies(double startMHz, double endMHz, double stepMHz)
{
    beginResetModel();
    m_points.clear();
    if (stepMHz <= 0 || startMHz >= endMHz)
        {
            endResetModel();
            return;
        }

    // Always add the precise start frequency
    m_points.append({startMHz, 0.0, false});

    // Start generating "nice" intermediate points
    double currentFreq = qCeil(startMHz / stepMHz) * stepMHz;
    if (currentFreq <= startMHz)
        {
            currentFreq += stepMHz;
        }

    while (currentFreq < endMHz)
        {
            m_points.append({currentFreq, 0.0, false});
            currentFreq += stepMHz;
        }

    // Always add the precise end frequency
    if (m_points.last().frequencyMHz < endMHz)
        {
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

const QVector<CalibrationPoint> &CalibrationModel::getPoints() const
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
    if (m_points.isEmpty())
        {
            return 0.0;
        }

    // Find two adjacent points to interpolate between
    const CalibrationPoint *p1 = nullptr;
    const CalibrationPoint *p2 = nullptr;
    const CalibrationPoint *firstSet = nullptr;
    const CalibrationPoint *lastSet = nullptr;

    // Find interpolation points and also the first/last calibrated points for extrapolation
    for (const auto &point : m_points)
        {
            if (point.isSet)
                {
                    if (!firstSet) firstSet = &point;
                    lastSet = &point;

                    if (point.frequencyMHz <= frequencyMHz)
                        {
                            p1 = &point;
                        }
                    if (point.frequencyMHz >= frequencyMHz && !p2)
                        {
                            p2 = &point;
                        }
                }
        }

    if (!p1 && !p2) return 0.0; // No set points at all
    if (!p1) return firstSet->correctionDb; // Extrapolate below range
    if (!p2) return lastSet->correctionDb;  // Extrapolate above range
    if (p1 == p2) return p1->correctionDb; // Exact match

    // Linear interpolation
    // Check for division by zero, though unlikely with the logic above
    if (p2->frequencyMHz == p1->frequencyMHz)
        {
            return p1->correctionDb;
        }

    double x1 = p1->frequencyMHz;
    double y1 = p1->correctionDb;
    double x2 = p2->frequencyMHz;
    double y2 = p2->correctionDb;

    return y1 + (y2 - y1) * (frequencyMHz - x1) / (x2 - x1);
}
