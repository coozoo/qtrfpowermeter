#pragma once

#include <QWidget>
#include <QList>
#include <QMap>

class CableModel;
class CableWidget;
class QCPGraph;
class QCPItemStraightLine;
class QCPItemTracer;
class QCustomPlot;
class QLineEdit;
class QComboBox;
class QPushButton;
class QVBoxLayout;
class QScrollArea;
class QLabel;
class QGridLayout;
class QResizeEvent;
class QToolButton;

class QtCoaxCableLossCalcManager : public QWidget
{
    Q_OBJECT

public:
    explicit QtCoaxCableLossCalcManager(QWidget *parent = nullptr);
    ~QtCoaxCableLossCalcManager();

    void loadCablesFromJson(QString configPath);

public slots:
    void setFrequency(double frequencyMHz);
    void setGlobalLength(double lengthM);
    void setPlotRange(double lower, double upper);
    void setIndividualLengthAllowed(bool allowed);
    void setAllowCableDupes(bool allowed) { m_allowCableDupes = allowed; }
    void setSilentCableDupes(bool silent);

signals:
    void totalAttenuationChanged(double totalDb);

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onAddCableClicked();
    void onDeleteMarkedClicked();
    void onClearAllClicked();
    void onSearchTextChanged(const QString &text);
    void onCurrentCableIndexChanged(int);
    void updateAttenuations();
    void replotGraphs();

private:
    void setupUi();
    void setupPlot();
    void addCableToPlot(CableWidget *cableWidget);
    void removeCableFromPlot(CableWidget *cableWidget);
    void updateGrid();
    void updateTracers();

    QMap<QString, CableModel *> m_cableModels;
    QList<CableWidget *> m_activeCableWidgets;
    double m_globalLength = 1.0;
    bool m_individualLengthAllowed = false;
    bool m_allowCableDupes = false;
    bool m_silentCableDupes = false;

    // UI Elements
    QVBoxLayout *m_mainLayout;
    QLineEdit *m_searchEdit;
    QComboBox *m_cableComboBox;
    QPushButton *m_addButton;
    QPushButton *m_deleteButton;
    QToolButton *m_clearAllButton;
    QScrollArea *m_scrollArea;
    QWidget *m_scrollAreaWidget;
    QGridLayout *m_cableWidgetsLayout;
    QCustomPlot *m_plot;
    QLabel *m_totalAttenuationLabel;

    QCPItemStraightLine *m_frequencyLine;
    QMap<QCPGraph *, QCPItemTracer *> m_extrapolationTracers;

    QList<QColor> uniColor={ QColor(40,110,255),  QColor(0,183,48),    QColor(255,0,0),
                              QColor(0,128,64),    QColor(255,200,20),  QColor(0,155,175),
                              QColor(145,0,140),   QColor(205,125,0),   QColor(0,0,0)};
    int m_nextColorIndex = 0;
};
