#ifndef FASTVIEWDIALOG_H
#define FASTVIEWDIALOG_H

#include <QDialog>
#include <QElapsedTimer>
#include <QTimer>
#include <deque>

class QCustomPlot;
class QLabel;
class QSpinBox;
class QDoubleSpinBox;
class QPushButton;
class QComboBox;

// Rolling raw-stream chart, opened from MainWindow on demand. Receives
// every measurement (no decimation) and renders the last
// `windowSeconds` seconds with min / max / average over the visible
// window. Useful at any device sample rate (10/40/640/1280 Hz) but
// pays for itself most at the faster rates that the long-term chart
// decimates away.
class FastViewDialog : public QDialog
{
    Q_OBJECT

public:
    enum class ScaleMode { Auto, Manual };

    explicit FastViewDialog(QWidget *parent = nullptr);
    ~FastViewDialog();

    // Base folder under which the silent Save-as-PNG / Save-as-CSV
    // actions drop their files. Mirrors MainWindow's per-session
    // `filepath`. Each save creates a fresh timestamped subfolder.
    void setSaveBaseDirectory(const QString &dir) { m_saveBaseDir = dir; }

    // Inform the dialog of the device's current sampling rate so it can
    // position samples at uniform x-intervals (the C# app does the same:
    // index-domain plotting, x = sample_index / sample_rate, so USB
    // burstiness doesn't squeeze or stretch the trace).
    void setSamplingRateHz(int hz);

public slots:
    // Feed one raw sample. `dbm` is the corrected-for-attenuation value
    // the main window already computed; the dialog stores it verbatim.
    void appendSample(double dbm);
    // Drop all stored samples and reset the time origin.
    void resetView();

private slots:
    void onWindowSecondsChanged(int seconds);
    void onDirectionToggled();
    void onRedrawTick();
    void onScaleModeChanged(int index);
    void onManualYChanged();
    void onPauseToggled(bool paused);
    void onSaveScreenshotClicked();
    void onSaveCsvClicked();

private:
    void trimToWindow(double nowSec);
    void refreshStatsLabel();
    void renderFrame();
    void updateYAxisRange(double windowMin, double windowMax);
    QString currentStatsText() const;

    QCustomPlot    *m_plot = nullptr;
    QLabel         *m_statsLabel = nullptr;
    QSpinBox       *m_windowSpin = nullptr;
    QPushButton    *m_directionButton = nullptr;
    QComboBox      *m_scaleModeCombo = nullptr;
    QDoubleSpinBox *m_yMinSpin = nullptr;
    QDoubleSpinBox *m_yMaxSpin = nullptr;
    QPushButton    *m_pauseButton = nullptr;

    QElapsedTimer m_clock;
    QTimer        m_redrawTimer;
    int  m_windowSeconds = 2;
    bool m_rightToLeft = true;     // classic scope: newest on the right
    bool m_paused = false;         // pause stops redraw + drops new samples
    ScaleMode m_scaleMode = ScaleMode::Auto;
    QString m_saveBaseDir;         // set by MainWindow; per-session folder
    int  m_sampleRateHz = 10;      // device sample rate; drives X spacing

    // Currently displayed Y range. Recomputed every frame in Auto mode
    // from the visible window's actual min/max (with a 5 dB margin and
    // integer snapping, matching the C# app's auto-scale behaviour) so
    // that fine detail returns when the signal stabilises. Held to the
    // user-set values in Manual mode.
    double m_yMinDisplayed = -60.0;
    double m_yMaxDisplayed = +10.0;

    // (elapsed_seconds_absolute, dbm) for the visible window. We keep
    // raw history bounded by windowSeconds so per-frame O(N) cost stays
    // small at the highest device rates (~2 K points at 1280 Hz / 2 s).
    std::deque<std::pair<double, double>> m_samples;
};

#endif // FASTVIEWDIALOG_H
