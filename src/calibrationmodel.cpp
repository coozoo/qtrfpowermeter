#include "calibrationmodel.h"

#include <QJsonArray>
#include <QJsonValue>
#include <algorithm>

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

/* linear
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
*/

double CalibrationModel::getCorrection(double frequencyMHz) const
{
    std::vector<double> x_points;
    std::vector<double> y_points;
    for (const auto &point : m_points)
        {
            if (point.isSet)
                {
                    x_points.push_back(point.frequencyMHz);
                    y_points.push_back(point.correctionDb);
                }
        }

    if (x_points.size() < 3)
        {
            if (x_points.empty())
            {
                return 0.0;
            }
            else if (x_points.size() == 1)
            {
                return y_points[0];
            }
            else
            {
                double x1 = x_points[0], y1 = y_points[0];
                double x2 = x_points[1], y2 = y_points[1];
                if (frequencyMHz <= x1)
                    return y1;
                if (frequencyMHz >= x2)
                    return y2;
                return y1 + (y2 - y1) * (frequencyMHz - x1) / (x2 - x1);
            }
        }

    tk::spline s;
    s.set_points(x_points, y_points, tk::spline::cspline);

    return s(frequencyMHz);
}

// =============================================================================
//  Mode handling
// =============================================================================

void CalibrationModel::setMode(Mode mode)
{
    if (m_mode == mode) return;
    m_mode = mode;
    // Qt-model surface only changes shape with rowCount/columnCount when
    // we add Advanced columns later (5f).  For now a layout signal is
    // enough to make any attached view refresh against the same shape.
    beginResetModel();
    endResetModel();
    emit modeChanged(m_mode);
}

QString CalibrationModel::modeToString(Mode mode)
{
    switch (mode) {
    case Mode::Simple:   return QStringLiteral("simple");
    case Mode::Advanced: return QStringLiteral("advanced");
    case Mode::Disabled: return QStringLiteral("disabled");
    }
    return QStringLiteral("simple");
}

CalibrationModel::Mode CalibrationModel::modeFromString(const QString &s)
{
    const QString k = s.trimmed().toLower();
    if (k == "advanced") return Mode::Advanced;
    if (k == "disabled") return Mode::Disabled;
    return Mode::Simple;
}

// =============================================================================
//  Mode-aware correction lookup
// =============================================================================

double CalibrationModel::getCorrection(double frequencyMHz, double measuredDbm) const
{
    switch (m_mode) {
    case Mode::Disabled:
        return 0.0;
    case Mode::Simple:
        Q_UNUSED(measuredDbm);
        return getCorrection(frequencyMHz);
    case Mode::Advanced:
        return interpolateAdvanced(frequencyMHz, measuredDbm);
    }
    return 0.0;
}

// =============================================================================
//  Advanced storage
// =============================================================================

void CalibrationModel::setAdvancedPowerAxis(const QVector<double> &powerAxisDbm)
{
    // Snapshot the old axis BEFORE assigning the new one so we can map
    // each cell back to its dBm value. A naive positional resize would
    // mean: if the old axis was [-40 .. +10] and the new is [-70 .. +10],
    // the cell that used to be at -40 (index 0) would end up at -70
    // (still index 0). Map by value to keep the association.
    const QVector<double> oldAxis = m_advancedPowerAxis;

    QVector<double> newAxis = powerAxisDbm;
    std::sort(newAxis.begin(), newAxis.end());
    m_advancedPowerAxis = newAxis;

    auto findOldIndex = [&oldAxis](double v) -> int {
        for (int i = 0; i < oldAxis.size(); ++i) {
            if (qAbs(oldAxis[i] - v) < 1e-6) return i;
        }
        return -1;
    };

    for (auto &row : m_advancedRows) {
        QVector<AdvancedCalibrationCell> resized(newAxis.size());
        for (int newIdx = 0; newIdx < newAxis.size(); ++newIdx) {
            const int oldIdx = findOldIndex(newAxis[newIdx]);
            if (oldIdx >= 0 && oldIdx < row.cells.size()) {
                resized[newIdx] = row.cells[oldIdx];
            }
        }
        row.cells = std::move(resized);
    }
    emit advancedTableChanged();
}

void CalibrationModel::setAdvancedStepDb(double stepDb)
{
    // Floor at 2.5 dB per the design lock; finer steps are not enforced
    // because some users might want regenerate-on-write later.
    if (stepDb < 2.5) stepDb = 2.5;
    m_advancedStepDb = stepDb;
}

void CalibrationModel::generateAdvancedPowerAxis(double minDbm, double maxDbm, double stepDb)
{
    if (stepDb < 2.5) stepDb = 2.5;
    m_advancedStepDb = stepDb;
    QVector<double> axis;
    if (maxDbm < minDbm) std::swap(minDbm, maxDbm);
    double v = minDbm;
    while (v <= maxDbm + 1e-9) {
        axis.append(v);
        v += stepDb;
    }
    setAdvancedPowerAxis(axis);
}

void CalibrationModel::setAdvancedRows(const QVector<AdvancedCalibrationRow> &rows)
{
    m_advancedRows = rows;
    std::sort(m_advancedRows.begin(), m_advancedRows.end(),
              [](const AdvancedCalibrationRow &a, const AdvancedCalibrationRow &b){
                  return a.frequencyMHz < b.frequencyMHz;
              });
    for (auto &row : m_advancedRows) {
        if (row.cells.size() != m_advancedPowerAxis.size()) {
            QVector<AdvancedCalibrationCell> resized(m_advancedPowerAxis.size());
            const int common = qMin(resized.size(), row.cells.size());
            for (int i = 0; i < common; ++i) resized[i] = row.cells[i];
            row.cells = std::move(resized);
        }
    }
    emit advancedTableChanged();
}

void CalibrationModel::generateAdvancedFrequencies(double startMHz, double endMHz, double stepMHz)
{
    if (stepMHz <= 0 || startMHz >= endMHz) return;

    beginResetModel();
    m_advancedRows.clear();

    auto pushFreq = [this](double f) {
        AdvancedCalibrationRow row;
        row.frequencyMHz = f;
        row.cells.resize(m_advancedPowerAxis.size());
        m_advancedRows.append(row);
    };

    // Always include the precise start, then "nice" step-aligned points,
    // then the precise end. Same algorithm as generateFrequencies.
    pushFreq(startMHz);
    double currentFreq = qCeil(startMHz / stepMHz) * stepMHz;
    if (currentFreq <= startMHz) currentFreq += stepMHz;
    while (currentFreq < endMHz) {
        pushFreq(currentFreq);
        currentFreq += stepMHz;
    }
    if (m_advancedRows.last().frequencyMHz < endMHz) {
        pushFreq(endMHz);
    }

    endResetModel();
    emit advancedTableChanged();
}

void CalibrationModel::addAdvancedFrequency(double frequencyMHz)
{
    for (const auto &r : m_advancedRows) {
        if (qFuzzyCompare(r.frequencyMHz, frequencyMHz)) return;
    }
    AdvancedCalibrationRow row;
    row.frequencyMHz = frequencyMHz;
    row.cells.resize(m_advancedPowerAxis.size());
    m_advancedRows.append(row);
    std::sort(m_advancedRows.begin(), m_advancedRows.end(),
              [](const AdvancedCalibrationRow &a, const AdvancedCalibrationRow &b){
                  return a.frequencyMHz < b.frequencyMHz;
              });
    emit advancedTableChanged();
}

void CalibrationModel::removeAdvancedFrequency(double frequencyMHz)
{
    for (int i = 0; i < m_advancedRows.size(); ++i) {
        if (qFuzzyCompare(m_advancedRows[i].frequencyMHz, frequencyMHz)) {
            m_advancedRows.removeAt(i);
            emit advancedTableChanged();
            return;
        }
    }
}

bool CalibrationModel::setAdvancedCell(int rowIdx, int powerIdx, double correctionDb)
{
    if (rowIdx < 0 || rowIdx >= m_advancedRows.size()) return false;
    auto &cells = m_advancedRows[rowIdx].cells;
    if (powerIdx < 0 || powerIdx >= cells.size()) return false;
    cells[powerIdx].correctionDb = correctionDb;
    cells[powerIdx].isSet = true;
    emit advancedTableChanged();
    return true;
}

void CalibrationModel::clearAdvancedCell(int rowIdx, int powerIdx)
{
    if (rowIdx < 0 || rowIdx >= m_advancedRows.size()) return;
    auto &cells = m_advancedRows[rowIdx].cells;
    if (powerIdx < 0 || powerIdx >= cells.size()) return;
    cells[powerIdx] = AdvancedCalibrationCell{};
    emit advancedTableChanged();
}

void CalibrationModel::clearAdvanced()
{
    m_advancedRows.clear();
    emit advancedTableChanged();
}

// =============================================================================
//  Bilinear interpolation across the Advanced grid
// =============================================================================

namespace
{
// Return the populated cell value nearest to powerIdx in the given row.
// Walks outward; returns false if nothing in the row is set.
bool nearestSetInRow(const AdvancedCalibrationRow &row, int powerIdx,
                     double &out)
{
    if (powerIdx < 0 || row.cells.isEmpty()) return false;
    const int n = row.cells.size();
    for (int offset = 0; offset < n; ++offset) {
        const int a = powerIdx - offset;
        const int b = powerIdx + offset;
        if (a >= 0 && a < n && row.cells[a].isSet) {
            out = row.cells[a].correctionDb;
            return true;
        }
        if (offset != 0 && b >= 0 && b < n && row.cells[b].isSet) {
            out = row.cells[b].correctionDb;
            return true;
        }
    }
    return false;
}

// Pick the bracketing power indices for measuredDbm against a sorted axis.
// Falls back to first/last when measuredDbm is outside the axis.
void bracketPower(const QVector<double> &axis, double measuredDbm,
                  int &lo, int &hi)
{
    if (axis.isEmpty()) { lo = hi = -1; return; }
    if (measuredDbm <= axis.first()) { lo = hi = 0; return; }
    if (measuredDbm >= axis.last())  { lo = hi = axis.size() - 1; return; }
    for (int i = 1; i < axis.size(); ++i) {
        if (axis[i] >= measuredDbm) { lo = i - 1; hi = i; return; }
    }
    lo = hi = axis.size() - 1;
}

// Same but for the freq axis built from the rows.
void bracketFreq(const QVector<AdvancedCalibrationRow> &rows, double freqMHz,
                 int &lo, int &hi)
{
    if (rows.isEmpty()) { lo = hi = -1; return; }
    if (freqMHz <= rows.first().frequencyMHz) { lo = hi = 0; return; }
    if (freqMHz >= rows.last().frequencyMHz)  { lo = hi = rows.size() - 1; return; }
    for (int i = 1; i < rows.size(); ++i) {
        if (rows[i].frequencyMHz >= freqMHz) { lo = i - 1; hi = i; return; }
    }
    lo = hi = rows.size() - 1;
}
} // namespace

double CalibrationModel::interpolateAdvanced(double freqMHz, double measuredDbm) const
{
    if (m_advancedRows.isEmpty() || m_advancedPowerAxis.isEmpty()) return 0.0;

    int fLo, fHi, pLo, pHi;
    bracketFreq(m_advancedRows, freqMHz, fLo, fHi);
    bracketPower(m_advancedPowerAxis, measuredDbm, pLo, pHi);

    // Fetch the four corners. If a cell is unfilled, fall back to the
    // nearest populated cell in the same row; if even that does not
    // exist the row contributes 0. Mirrors the "auto-fill leaves holes
    // and the apply path interpolates across them" decision.
    auto cellValue = [&](int rowIdx, int powIdx) -> double {
        const auto &row = m_advancedRows[rowIdx];
        if (powIdx >= 0 && powIdx < row.cells.size()
            && row.cells[powIdx].isSet) {
            return row.cells[powIdx].correctionDb;
        }
        double nearest;
        if (nearestSetInRow(row, powIdx, nearest)) return nearest;
        return 0.0;
    };

    const double v00 = cellValue(fLo, pLo);
    const double v01 = cellValue(fLo, pHi);
    const double v10 = cellValue(fHi, pLo);
    const double v11 = cellValue(fHi, pHi);

    // Linear weights along power and freq axes; degenerate cases (equal
    // brackets) reduce naturally to nearest-edge.
    double tp = 0.0;
    if (pLo != pHi) {
        const double p0 = m_advancedPowerAxis[pLo];
        const double p1 = m_advancedPowerAxis[pHi];
        if (p1 > p0) tp = (measuredDbm - p0) / (p1 - p0);
    }
    double tf = 0.0;
    if (fLo != fHi) {
        const double f0 = m_advancedRows[fLo].frequencyMHz;
        const double f1 = m_advancedRows[fHi].frequencyMHz;
        if (f1 > f0) tf = (freqMHz - f0) / (f1 - f0);
    }
    tp = qBound(0.0, tp, 1.0);
    tf = qBound(0.0, tf, 1.0);

    const double a = v00 * (1.0 - tp) + v01 * tp;
    const double b = v10 * (1.0 - tp) + v11 * tp;
    return a * (1.0 - tf) + b * tf;
}

// =============================================================================
//  JSON profile I/O
// =============================================================================

QJsonObject CalibrationModel::saveToJson() const
{
    QJsonObject root;
    root["mode"] = modeToString(m_mode);

    QJsonArray simpleArr;
    for (const auto &p : m_points) {
        QJsonObject o;
        o["frequencyMHz"] = p.frequencyMHz;
        o["correctionDb"] = p.correctionDb;
        o["isSet"]        = p.isSet;
        simpleArr.append(o);
    }
    root["simple"] = simpleArr;

    QJsonObject adv;
    adv["stepDb"] = m_advancedStepDb;
    QJsonArray axisArr;
    for (double v : m_advancedPowerAxis) axisArr.append(v);
    adv["powerAxisDbm"] = axisArr;
    QJsonArray rowsArr;
    for (const auto &row : m_advancedRows) {
        QJsonObject ro;
        ro["frequencyMHz"] = row.frequencyMHz;
        QJsonArray cellsArr;
        for (const auto &c : row.cells) {
            QJsonObject co;
            co["dB"]  = c.correctionDb;
            co["set"] = c.isSet;
            cellsArr.append(co);
        }
        ro["cells"] = cellsArr;
        rowsArr.append(ro);
    }
    adv["rows"] = rowsArr;
    root["advanced"] = adv;
    return root;
}

bool CalibrationModel::loadFromJson(const QJsonObject &root)
{
    // Legacy profile detection: the old format had a top-level "points"
    // array and no "mode" / "simple" / "advanced" fields. Migrate by
    // mapping points -> simple and forcing Mode::Simple.
    const bool legacy = !root.contains("mode")
                        && !root.contains("simple")
                        && !root.contains("advanced")
                        && root.contains("points");

    QVector<CalibrationPoint> simpleIn;
    QVector<double>           axisIn;
    double                    stepIn = 2.5;
    QVector<AdvancedCalibrationRow> rowsIn;
    Mode modeIn = Mode::Simple;

    if (legacy) {
        const QJsonArray arr = root["points"].toArray();
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            CalibrationPoint p;
            p.frequencyMHz = o["frequencyMHz"].toDouble();
            p.correctionDb = o["correctionDb"].toDouble();
            p.isSet        = o["isSet"].toBool();
            simpleIn.append(p);
        }
    } else {
        modeIn = modeFromString(root["mode"].toString("simple"));

        const QJsonArray simpleArr = root["simple"].toArray();
        for (const QJsonValue &v : simpleArr) {
            const QJsonObject o = v.toObject();
            CalibrationPoint p;
            p.frequencyMHz = o["frequencyMHz"].toDouble();
            p.correctionDb = o["correctionDb"].toDouble();
            p.isSet        = o["isSet"].toBool();
            simpleIn.append(p);
        }
        if (root.contains("advanced")) {
            const QJsonObject adv = root["advanced"].toObject();
            stepIn = adv["stepDb"].toDouble(2.5);
            const QJsonArray axisArr = adv["powerAxisDbm"].toArray();
            for (const QJsonValue &v : axisArr) axisIn.append(v.toDouble());
            const QJsonArray rowsArr = adv["rows"].toArray();
            for (const QJsonValue &rv : rowsArr) {
                const QJsonObject ro = rv.toObject();
                AdvancedCalibrationRow row;
                row.frequencyMHz = ro["frequencyMHz"].toDouble();
                const QJsonArray cellsArr = ro["cells"].toArray();
                for (const QJsonValue &cv : cellsArr) {
                    const QJsonObject co = cv.toObject();
                    AdvancedCalibrationCell c;
                    c.correctionDb = co["dB"].toDouble();
                    c.isSet        = co["set"].toBool();
                    row.cells.append(c);
                }
                rowsIn.append(row);
            }
        }
    }

    setPoints(simpleIn);
    m_advancedStepDb = stepIn;
    setAdvancedPowerAxis(axisIn);
    setAdvancedRows(rowsIn);
    setMode(modeIn);
    return true;
}
