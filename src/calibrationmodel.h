#ifndef CALIBRATIONMODEL_H
#define CALIBRATIONMODEL_H

#include <QAbstractTableModel>
#include <QJsonObject>
#include <QVector>
#include <QColor>
#include <QtMath>
#include "calibrationpoint.h"
#include "spline.h"
#include <vector>

// Single cell of an Advanced (2D) calibration table. One per (freq, power)
// pair. Mirrors CalibrationPoint::isSet so unfilled cells can be excluded
// from interpolation.
struct AdvancedCalibrationCell
{
    double correctionDb = 0.0;
    bool   isSet = false;
};

// Whole row of the Advanced calibration table at a single freq.
struct AdvancedCalibrationRow
{
    double frequencyMHz = 0.0;
    QVector<AdvancedCalibrationCell> cells;  // one per powerAxisDbm entry
};

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

    // Tagged-union mode. Default Simple to keep every existing caller
    // (and the legacy JSON profile format) working as-is.
    //   Simple    - per-freq single correction. Existing UI; existing
    //                getCorrection(freqMHz) is the right lookup.
    //   Advanced  - 2D table (freq, power) -> correction. Bilinear interp
    //                on (freq, measured_dbm).  Same shape as ConceptRF's
    //                voltage table, but with user-chosen freqs and a
    //                user-chosen power step.
    //   Disabled  - no calibration applied. getCorrection returns 0.
    enum class Mode { Simple, Advanced, Disabled };
    Q_ENUM(Mode)

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    // String form for JSON round-trip.
    static QString modeToString(Mode mode);
    static Mode    modeFromString(const QString &s);

    // Basic model functions (Qt model surface still exposes Simple-mode
    // shape: rows = freqs, two columns Freq / Correction. The Advanced
    // editor in 5f uses its own grid model on top of advancedRows()).
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Simple-mode API (existing). Kept verbatim so all existing tests and
    // call sites keep working.
    void generateFrequencies(double startMHz, double endMHz, double stepMHz);
    void clear();
    const QVector<CalibrationPoint> &getPoints() const;
    void setPoints(const QVector<CalibrationPoint> &points);
    // Simple-mode lookup. Cubic spline (or linear fallback) across the
    // populated CalibrationPoints.  Untouched by mode changes - call site
    // can still use this when explicitly asking for the Simple value.
    double getCorrection(double frequencyMHz) const;

    // Mode-aware lookup. The right entry point for the runtime apply
    // path. Simple mode ignores measuredDbm. Disabled returns 0.
    // Advanced does bilinear interp on (freq, measured_dbm) across the
    // populated cells, with nearest-edge fallback at the boundaries.
    double getCorrection(double frequencyMHz, double measuredDbm) const;

    // Advanced-mode API. The 2D table is stored independently from the
    // Simple points; flipping modes does not touch either store, so the
    // user's work is preserved across switches.
    void   setAdvancedPowerAxis(const QVector<double> &powerAxisDbm);
    const  QVector<double> &advancedPowerAxis() const { return m_advancedPowerAxis; }
    double advancedStepDb() const { return m_advancedStepDb; }
    void   setAdvancedStepDb(double stepDb);
    void   generateAdvancedPowerAxis(double minDbm, double maxDbm, double stepDb);

    const QVector<AdvancedCalibrationRow> &advancedRows() const { return m_advancedRows; }
    void  setAdvancedRows(const QVector<AdvancedCalibrationRow> &rows);
    void  addAdvancedFrequency(double frequencyMHz);
    void  removeAdvancedFrequency(double frequencyMHz);
    // Mirror of generateFrequencies for the Advanced rows: always
    // includes the exact start and end, fills "nice" stepMHz-aligned
    // points in between (ceil(start/step)*step, etc.). Replaces any
    // existing rows.
    void  generateAdvancedFrequencies(double startMHz, double endMHz, double stepMHz);
    bool  setAdvancedCell(int rowIdx, int powerIdx, double correctionDb);
    void  clearAdvancedCell(int rowIdx, int powerIdx);
    void  clearAdvanced();

    // JSON profile I/O. New schema has a top-level "mode" field plus
    // "simple" and "advanced" subobjects. Legacy profiles without "mode"
    // auto-migrate to Simple and the old "points" array maps to Simple's
    // CalibrationPoint list. saveToJson always emits the new schema.
    QJsonObject saveToJson() const;
    bool        loadFromJson(const QJsonObject &root);

signals:
    void modeChanged(CalibrationModel::Mode mode);
    void advancedTableChanged();

private:
    double  interpolateAdvanced(double frequencyMHz, double measuredDbm) const;

    Mode    m_mode = Mode::Simple;

    // Simple data (existing).
    QVector<CalibrationPoint> m_points;

    // Advanced data.
    QVector<double>                       m_advancedPowerAxis;   // sorted, regular step
    double                                m_advancedStepDb = 2.5; // for display/regen
    QVector<AdvancedCalibrationRow>       m_advancedRows;        // sorted by freq
};

#endif // CALIBRATIONMODEL_H
