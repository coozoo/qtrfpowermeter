#include <QtTest/QtTest>
#include <QSignalSpy>
#include "advancedcalibrationtablemodel.h"
#include "calibrationmodel.h"

// Pure-logic tests for the Advanced grid view adapter. Drives it through
// the public CalibrationModel surface; no GUI involved.
class TestAdvancedCalibrationAdapter : public QObject
{
    Q_OBJECT
private slots:
    void emptySource_zeroDimensions();
    void axisOnly_zeroRowsButColumnsMatch();
    void rowsAndAxis_dimensionsMatch();
    void unsetCellShowsDash();
    void setCellThroughAdapter_writesBackToModel();
    void setEmptyStringClearsCell();
    void setGarbageReturnsFalse();
    void headerData_powerColumnsAndFreqRows();
    void flags_editableEnabledSelectable();
    void sourceReset_propagatesAsAdapterReset();
};

void TestAdvancedCalibrationAdapter::emptySource_zeroDimensions()
{
    CalibrationModel m;
    AdvancedCalibrationTableModel a(&m);
    QCOMPARE(a.rowCount(), 0);
    QCOMPARE(a.columnCount(), 0);
}

void TestAdvancedCalibrationAdapter::axisOnly_zeroRowsButColumnsMatch()
{
    CalibrationModel m;
    AdvancedCalibrationTableModel a(&m);
    m.generateAdvancedPowerAxis(-40.0, 10.0, 2.5);
    QCOMPARE(a.rowCount(), 0);
    QCOMPARE(a.columnCount(), m.advancedPowerAxis().size());
}

void TestAdvancedCalibrationAdapter::rowsAndAxis_dimensionsMatch()
{
    CalibrationModel m;
    AdvancedCalibrationTableModel a(&m);
    m.generateAdvancedPowerAxis(-40.0, 10.0, 2.5);
    m.addAdvancedFrequency(100.0);
    m.addAdvancedFrequency(500.0);
    m.addAdvancedFrequency(1000.0);
    QCOMPARE(a.rowCount(), 3);
    QCOMPARE(a.columnCount(), m.advancedPowerAxis().size());
}

void TestAdvancedCalibrationAdapter::unsetCellShowsDash()
{
    CalibrationModel m;
    AdvancedCalibrationTableModel a(&m);
    m.generateAdvancedPowerAxis(-10.0, 10.0, 5.0);
    m.addAdvancedFrequency(100.0);
    QCOMPARE(a.data(a.index(0, 0), Qt::DisplayRole).toString(), QString("-"));
    QVERIFY(!a.data(a.index(0, 0), Qt::EditRole).isValid());
}

void TestAdvancedCalibrationAdapter::setCellThroughAdapter_writesBackToModel()
{
    CalibrationModel m;
    AdvancedCalibrationTableModel a(&m);
    m.generateAdvancedPowerAxis(-10.0, 10.0, 5.0);
    m.addAdvancedFrequency(100.0);
    QSignalSpy changed(&a, &QAbstractItemModel::dataChanged);

    QVERIFY(a.setData(a.index(0, 2), QVariant(1.25), Qt::EditRole));
    QCOMPARE(changed.count(), 1);

    // Read back via the model directly.
    QVERIFY(m.advancedRows().value(0).cells.value(2).isSet);
    QCOMPARE(m.advancedRows().value(0).cells.value(2).correctionDb, 1.25);
    // And via the adapter's display.
    QCOMPARE(a.data(a.index(0, 2), Qt::DisplayRole).toString(),
             QString::number(1.25, 'f', 3));
}

void TestAdvancedCalibrationAdapter::setEmptyStringClearsCell()
{
    CalibrationModel m;
    AdvancedCalibrationTableModel a(&m);
    m.generateAdvancedPowerAxis(-10.0, 10.0, 5.0);
    m.addAdvancedFrequency(100.0);
    QVERIFY(a.setData(a.index(0, 0), QVariant(3.0), Qt::EditRole));
    QVERIFY(m.advancedRows().value(0).cells.value(0).isSet);

    QVERIFY(a.setData(a.index(0, 0), QVariant(QString("")), Qt::EditRole));
    QVERIFY(!m.advancedRows().value(0).cells.value(0).isSet);
    QCOMPARE(a.data(a.index(0, 0), Qt::DisplayRole).toString(), QString("-"));
}

void TestAdvancedCalibrationAdapter::setGarbageReturnsFalse()
{
    CalibrationModel m;
    AdvancedCalibrationTableModel a(&m);
    m.generateAdvancedPowerAxis(-10.0, 10.0, 5.0);
    m.addAdvancedFrequency(100.0);
    QVERIFY(!a.setData(a.index(0, 0), QVariant(QString("not a number")), Qt::EditRole));
    QVERIFY(!m.advancedRows().value(0).cells.value(0).isSet);
}

void TestAdvancedCalibrationAdapter::headerData_powerColumnsAndFreqRows()
{
    CalibrationModel m;
    AdvancedCalibrationTableModel a(&m);
    m.generateAdvancedPowerAxis(-10.0, 10.0, 5.0);
    m.addAdvancedFrequency(100.0);     // < 1000 MHz -> "MHz"
    m.addAdvancedFrequency(1200.0);    // >= 1000 MHz -> "GHz"

    const auto col0 = a.headerData(0, Qt::Horizontal, Qt::DisplayRole).toString();
    QVERIFY(col0.endsWith("dBm"));
    QVERIFY(col0.contains("-10"));

    const auto row0 = a.headerData(0, Qt::Vertical, Qt::DisplayRole).toString();
    QVERIFY(row0.endsWith("MHz"));
    QVERIFY(row0.contains("100"));

    const auto row1 = a.headerData(1, Qt::Vertical, Qt::DisplayRole).toString();
    QVERIFY(row1.endsWith("GHz"));
    QVERIFY(row1.contains("1.200"));
}

void TestAdvancedCalibrationAdapter::flags_editableEnabledSelectable()
{
    CalibrationModel m;
    AdvancedCalibrationTableModel a(&m);
    m.generateAdvancedPowerAxis(-10.0, 10.0, 5.0);
    m.addAdvancedFrequency(100.0);
    const auto f = a.flags(a.index(0, 0));
    QVERIFY(f & Qt::ItemIsEditable);
    QVERIFY(f & Qt::ItemIsEnabled);
    QVERIFY(f & Qt::ItemIsSelectable);
}

void TestAdvancedCalibrationAdapter::sourceReset_propagatesAsAdapterReset()
{
    CalibrationModel m;
    AdvancedCalibrationTableModel a(&m);
    m.generateAdvancedPowerAxis(-10.0, 10.0, 5.0);
    m.addAdvancedFrequency(100.0);

    QSignalSpy resetSpy(&a, &QAbstractItemModel::modelReset);
    m.clearAdvanced();
    QVERIFY(resetSpy.count() >= 1);
    QCOMPARE(a.rowCount(), 0);
}

QTEST_MAIN(TestAdvancedCalibrationAdapter)
#include "test_advanced_calibration_adapter.moc"
