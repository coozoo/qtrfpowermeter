#ifndef CALIBRATIONMODEL_H
#define CALIBRATIONMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QColor>
#include <QtMath>
#include "calibrationpoint.h"
#include "spline.h"
#include <vector>

class CalibrationModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit CalibrationModel(QObject *parent = nullptr);

    enum Column
    {
        Frequency = 0,
        Correction,
        ColumnCount
    };

    // Basic model functions
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Public API to manage data
    void generateFrequencies(double startMHz, double endMHz, double stepMHz);
    void clear();
    const QVector<CalibrationPoint> &getPoints() const;
    void setPoints(const QVector<CalibrationPoint> &points);
    double getCorrection(double frequencyMHz) const;

private:
    QVector<CalibrationPoint> m_points;
};

#endif // CALIBRATIONMODEL_H
