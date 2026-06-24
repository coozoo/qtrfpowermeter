#ifndef ADVANCEDCALIBRATIONTABLEMODEL_H
#define ADVANCEDCALIBRATIONTABLEMODEL_H

#include <QAbstractTableModel>

class CalibrationModel;

// Qt view adapter on top of CalibrationModel's Advanced 2D store.
// Rows = m_advancedRows (sorted by freq); columns = m_advancedPowerAxis
// entries. Cell display shows correctionDb (3 decimals) or "-" if unset.
// Editing writes back via setAdvancedCell(); editing a cell to an empty
// string clears it via clearAdvancedCell().
//
// The adapter does NOT own the underlying data. It listens to the
// source model's reset / advancedTableChanged signal and re-emits the
// appropriate layoutChanged so attached views refresh.
class AdvancedCalibrationTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit AdvancedCalibrationTableModel(CalibrationModel *source,
                                           QObject *parent = nullptr);

    int      rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int      columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool     setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private slots:
    void onSourceChanged();

private:
    CalibrationModel *m_source;
};

#endif
