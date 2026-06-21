#include "conceptrfrpmlookuptables.h"
#include <QDebug>
#include <cmath>

// =============================================================================
// Abstract Base Class Implementation
// =============================================================================

double AbstractRpmLookupTable::adcToVoltage(qint32 rawAdc) const
{
    return (static_cast<double>(rawAdc) * 1500.0) / 8388607.0;
}

void AbstractRpmLookupTable::setVoltageRow(int freqIndex, const QVector<float>& voltages)
{
    if (freqIndex >= 0 && freqIndex < m_voltageTableMv.size()) {
        m_voltageTableMv[freqIndex] = voltages;
    }
}

bool AbstractRpmLookupTable::isRowFilled(int freqIndex) const
{
    if (freqIndex < 0 || freqIndex >= m_voltageTableMv.size()) return false;
    const auto &row = m_voltageTableMv[freqIndex];
    if (row.size() != m_powerAxisDb.size()) return false;
    // initializeTables() fills rows with 0.0f; a downloaded row will always
    // have at least one non-zero detector reading.
    for (float v : row) if (v != 0.0f) return true;
    return false;
}

int AbstractRpmLookupTable::firstUnfilledRow() const
{
    for (int i = 0; i < m_voltageTableMv.size(); ++i) {
        if (!isRowFilled(i)) return i;
    }
    return -1;
}

bool AbstractRpmLookupTable::allRowsFilled() const
{
    return firstUnfilledRow() < 0;
}

double AbstractRpmLookupTable::convert(qint32 rawAdc, quint64 freqHz) const
{
    if (m_freqAxisHz.isEmpty() || m_powerAxisDb.isEmpty() || m_voltageTableMv.isEmpty()) {
        return -999.9; // Tables not initialized
    }

    float voltage = static_cast<float>(adcToVoltage(rawAdc));
    float power_out1 = 100000.0f;
    float power_out2 = 100000.0f;
    int num1 = 0, num2 = 0;

    if (freqHz < m_freqAxisHz.first() || freqHz > m_freqAxisHz.last()) {
        return 100000.0f;
    }

    for (int i = 0; i < m_freqAxisHz.size() - 1; i++) {
        if (freqHz >= m_freqAxisHz[i] && freqHz <= m_freqAxisHz[i + 1]) {
            num1 = i;
            num2 = i + 1;
            break;
        }
    }
    
    // Check for empty voltage rows (not yet downloaded)
    if (m_voltageTableMv[num1].isEmpty() || m_voltageTableMv[num2].isEmpty()) {
        return -999.8; // Calibration data not ready
    }

    if (voltage >= m_voltageTableMv[num1].last() || voltage >= m_voltageTableMv[num2].last()) {
        return 100000.0f;
    }

    if (voltage < m_voltageTableMv[num1].first() || voltage < m_voltageTableMv[num2].first()) {
        // This edge case logic seems unusual, but it's a direct translation.
        power_out1 = m_powerAxisDb[0] + voltage * (m_powerAxisDb[0] - 3000.0f);
        power_out2 = m_powerAxisDb[0] + voltage * (m_powerAxisDb[0] - 3000.0f);
    } else {
        for (int j = 0; j < m_powerAxisDb.size() - 1; j++) {
            if (voltage >= m_voltageTableMv[num1][j] && voltage <= m_voltageTableMv[num1][j + 1]) {
                float v1 = m_voltageTableMv[num1][j];
                float v2 = m_voltageTableMv[num1][j + 1];
                power_out1 = m_powerAxisDb[j] + (voltage - v1) / (v2 - v1) * (m_powerAxisDb[j + 1] - m_powerAxisDb[j]);
                break;
            }
        }
        for (int k = 0; k < m_powerAxisDb.size() - 1; k++) {
            if (voltage >= m_voltageTableMv[num2][k] && voltage <= m_voltageTableMv[num2][k + 1]) {
                 float v1 = m_voltageTableMv[num2][k];
                 float v2 = m_voltageTableMv[num2][k + 1];
                power_out2 = m_powerAxisDb[k] + (voltage - v1) / (v2 - v1) * (m_powerAxisDb[k + 1] - m_powerAxisDb[k]);
                break;
            }
        }
    }
    
    // This logic appears to be a safeguard against non-monotonic data.
    if (power_out1 < m_powerAxisDb.first()) {
        power_out1 = m_powerAxisDb.first() - (power_out1 - std::floor(power_out1));
    }
    if (power_out2 < m_powerAxisDb.first()) {
        power_out2 = m_powerAxisDb.first() - (power_out2 - std::floor(power_out2));
    }

    double proportion = static_cast<double>(freqHz - m_freqAxisHz[num1]) / static_cast<double>(m_freqAxisHz[num2] - m_freqAxisHz[num1]);
    if (proportion > 1.0) {
        proportion = 1.0;
    }

    return static_cast<float>(static_cast<double>(power_out1) * (1.0 - proportion) + static_cast<double>(power_out2) * proportion);
}


// =============================================================================
// Concrete Class Implementations
// =============================================================================

void Rpm20gsLookupTable::initializeTables()
{
    // Logic from lookup_table_101_ARW28340.cs and lookup_table_104_ARW28340.cs
    int freq_size = 202; // For device ID 101
    // int freq_size = 218; // For device ID 104
    int power_size = 21;

    m_freqAxisHz.resize(freq_size);
    m_powerAxisDb.resize(power_size);
    m_voltageTableMv.resize(freq_size);
    for(int i = 0; i < freq_size; ++i) {
        m_voltageTableMv[i].resize(power_size);
        m_voltageTableMv[i].fill(0.0f);
    }
    
    m_freqAxisHz[0] = 10000000L;
    m_freqAxisHz[1] = 50000000L;
    for (int k = 2; k < freq_size; k++) {
        m_freqAxisHz[k] = 100000000L * (k - 1);
    }

    for (int l = 0; l < power_size; l++) {
        m_powerAxisDb[l] = static_cast<float>(l) * 2.5f + -40.0f;
    }
}

void Rpm3gsLookupTable::initializeTables()
{
    // Logic from lookup_table_102_AD8362.cs and lookup_table_105_AD8362.cs
    int freq_size = 33; // For device ID 102
    // int freq_size = 58; // For device ID 105
    int power_size = 25;

    m_freqAxisHz.resize(freq_size);
    m_powerAxisDb.resize(power_size);
    m_voltageTableMv.resize(freq_size);
    for(int i = 0; i < freq_size; ++i) {
        m_voltageTableMv[i].resize(power_size);
        m_voltageTableMv[i].fill(0.0f);
    }

    m_freqAxisHz[0] = 50L;
    m_freqAxisHz[1] = 10000000L;
    m_freqAxisHz[2] = 50000000L;
    for (int k = 3; k < freq_size; k++) {
        m_freqAxisHz[k] = 100000000L * (k - 2);
    }

    for (int l = 0; l < power_size; l++) {
        m_powerAxisDb[l] = static_cast<float>(l) * 2.5f + -50.0f;
    }
}

void Rpm9gLookupTable::initializeTables()
{
    // Logic from lookup_table_103_ARW22347.cs and lookup_table_106_ARW22347.cs
    int freq_size = 92; // For device ID 103
    // int freq_size = 99; // For device ID 106
    int power_size = 21;

    m_freqAxisHz.resize(freq_size);
    m_powerAxisDb.resize(power_size);
    m_voltageTableMv.resize(freq_size);
    for(int i = 0; i < freq_size; ++i) {
        m_voltageTableMv[i].resize(power_size);
        m_voltageTableMv[i].fill(0.0f);
    }
    
    m_freqAxisHz[0] = 10000000L;
    m_freqAxisHz[1] = 50000000L;
    for (int k = 2; k < freq_size; k++) {
        m_freqAxisHz[k] = 100000000L * (k - 1);
    }

    for (int l = 0; l < power_size; l++) {
        m_powerAxisDb[l] = static_cast<float>(l) * 2.5f + -40.0f;
    }
}

void Rpm6ghLookupTable::initializeTables()
{
    // Logic from lookup_table_107_ARW22283.cs
    int freq_size = 69;
    int power_size = 41;

    m_freqAxisHz.resize(freq_size);
    m_powerAxisDb.resize(power_size);
    m_voltageTableMv.resize(freq_size);
    for(int i = 0; i < freq_size; ++i) {
        m_voltageTableMv[i].resize(power_size);
        m_voltageTableMv[i].fill(0.0f);
    }
    
    for (int k = 0; k < 9; k++) {
        m_freqAxisHz[k] = 10000000 + 10000000L * k;
    }
    for (int l = 9; l < freq_size; l++) {
        m_freqAxisHz[l] = 100000000L * (l - 8);
    }

    for (int m = 0; m < power_size; m++) {
        m_powerAxisDb[m] = static_cast<float>(m) * 2.5f + -80.0f;
    }
}
