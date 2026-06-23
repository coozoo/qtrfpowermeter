#ifndef CONCEPTRFRPM_LOOKUPTABLES_H
#define CONCEPTRFRPM_LOOKUPTABLES_H

#include <QObject>
#include <QVector>

class AbstractRpmLookupTable
{
public:
    virtual ~AbstractRpmLookupTable() = default;

    double convert(qint32 rawAdc, quint64 freqHz) const;
    virtual void initializeTables() = 0;
    void setVoltageRow(int freqIndex, const QVector<float>& voltages);

    int getFreqTableSize() const { return m_freqAxisHz.size(); }
    int getPowerTableSize() const { return m_powerAxisDb.size(); }

    // Read-only accessors used by the device-calibration viewer.
    const QVector<quint64> &freqAxisHz() const { return m_freqAxisHz; }
    const QVector<float>   &powerAxisDb() const { return m_powerAxisDb; }
    const QVector<QVector<float>> &voltageTableMv() const { return m_voltageTableMv; }

    bool isRowFilled(int freqIndex) const;
    int firstUnfilledRow() const;
    bool allRowsFilled() const;

protected:
    double adcToVoltage(qint32 rawAdc) const;

    QVector<float> m_powerAxisDb;
    QVector<quint64> m_freqAxisHz;
    QVector<QVector<float>> m_voltageTableMv;
};


class Rpm20gsLookupTable : public AbstractRpmLookupTable {
public:
    void initializeTables() override;
};

class Rpm3gsLookupTable : public AbstractRpmLookupTable {
public:
    void initializeTables() override;
};

class Rpm9gLookupTable : public AbstractRpmLookupTable {
public:
    void initializeTables() override;
};

class Rpm6ghLookupTable : public AbstractRpmLookupTable {
public:
    void initializeTables() override;
};

#endif // CONCEPTRFRPM_LOOKUPTABLES_H
