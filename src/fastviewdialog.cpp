#include "fastviewdialog.h"
#include "qcustomplot.h"
#include "savedtoast.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QVector>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QPainter>
#include <QPixmap>
#include <QDateTime>
#include <QFrame>
#include <QFont>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr int kRedrawHz = 60;
}

FastViewDialog::FastViewDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Fast view (raw samples)"));
    resize(720, 360);

    m_plot = new QCustomPlot(this);
    m_plot->addGraph();
    m_plot->graph(0)->setPen(QPen(Qt::darkBlue));
    m_plot->xAxis->setLabel(tr("seconds ago"));   // updated on direction toggle
    m_plot->yAxis->setLabel(tr("dBm"));
    m_plot->xAxis->setRange(0, m_windowSeconds);
    m_plot->yAxis->setRange(m_yMinDisplayed, m_yMaxDisplayed);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setText(tr("min: -    max: -    avg: -    samples: 0"));

    m_windowSpin = new QSpinBox(this);
    m_windowSpin->setRange(1, 60);
    m_windowSpin->setValue(m_windowSeconds);
    m_windowSpin->setSuffix(tr(" s"));
    m_windowSpin->setToolTip(tr("Visible window length"));
    connect(m_windowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &FastViewDialog::onWindowSecondsChanged);

    m_directionButton = new QPushButton(this);
    m_directionButton->setCheckable(true);
    m_directionButton->setChecked(m_rightToLeft);
    m_directionButton->setToolTip(tr("Flow direction: new samples enter on the right, old on the left (toggle to reverse)"));
    connect(m_directionButton, &QPushButton::clicked,
            this, &FastViewDialog::onDirectionToggled);

    // Y-scale controls: Auto tracks the visible window's min/max with a
    // 5 dB margin (mirrors the C# app); Manual locks YMin/YMax to the
    // spinbox values so you can zoom into fine detail.
    m_scaleModeCombo = new QComboBox(this);
    m_scaleModeCombo->addItem(tr("Auto"));
    m_scaleModeCombo->addItem(tr("Manual"));
    m_scaleModeCombo->setToolTip(tr("Y-axis scaling mode"));
    connect(m_scaleModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FastViewDialog::onScaleModeChanged);

    m_yMinSpin = new QDoubleSpinBox(this);
    m_yMinSpin->setRange(-200.0, 100.0);
    m_yMinSpin->setDecimals(1);
    m_yMinSpin->setSingleStep(1.0);
    m_yMinSpin->setSuffix(tr(" dBm"));
    m_yMinSpin->setValue(m_yMinDisplayed);
    m_yMinSpin->setVisible(false);
    connect(m_yMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double){ onManualYChanged(); });

    m_yMaxSpin = new QDoubleSpinBox(this);
    m_yMaxSpin->setRange(-200.0, 100.0);
    m_yMaxSpin->setDecimals(1);
    m_yMaxSpin->setSingleStep(1.0);
    m_yMaxSpin->setSuffix(tr(" dBm"));
    m_yMaxSpin->setValue(m_yMaxDisplayed);
    m_yMaxSpin->setVisible(false);
    connect(m_yMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double){ onManualYChanged(); });

    m_pauseButton = new QPushButton(tr("Pause"), this);
    m_pauseButton->setCheckable(true);
    m_pauseButton->setToolTip(tr("Pause: freeze the chart and stop accepting new samples"));
    connect(m_pauseButton, &QPushButton::toggled, this, &FastViewDialog::onPauseToggled);

    QPushButton *saveButton = new QPushButton(tr("Save PNG"), this);
    saveButton->setToolTip(tr("Save the chart + stats line silently into the session folder"));
    connect(saveButton, &QPushButton::clicked, this, &FastViewDialog::onSaveScreenshotClicked);

    QPushButton *csvButton = new QPushButton(tr("Save CSV"), this);
    csvButton->setToolTip(tr("Save the visible window's samples as CSV into the session folder"));
    connect(csvButton, &QPushButton::clicked, this, &FastViewDialog::onSaveCsvClicked);

    QPushButton *resetButton = new QPushButton(tr("Reset"), this);
    connect(resetButton, &QPushButton::clicked, this, &FastViewDialog::resetView);

    QPushButton *closeButton = new QPushButton(tr("Close"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    // Layout: chart, then a stats row, then a controls row. Putting stats
    // on its own line stops the controls row from growing past the
    // window width when Manual mode shows the YMin/YMax spinboxes.
    QHBoxLayout *statsRow = new QHBoxLayout;
    statsRow->addWidget(m_statsLabel);
    statsRow->addStretch(1);

    QHBoxLayout *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(tr("Window:"), this));
    controls->addWidget(m_windowSpin);
    controls->addWidget(m_directionButton);
    controls->addSpacing(8);
    controls->addWidget(new QLabel(tr("Y:"), this));
    controls->addWidget(m_scaleModeCombo);
    controls->addWidget(m_yMinSpin);
    controls->addWidget(m_yMaxSpin);
    controls->addStretch(1);
    controls->addWidget(m_pauseButton);
    controls->addWidget(saveButton);
    controls->addWidget(csvButton);
    controls->addWidget(resetButton);
    controls->addWidget(closeButton);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_plot, 1);
    layout->addLayout(statsRow);
    layout->addLayout(controls);

    // Sync the direction label / xAxis label now that everything exists.
    // onDirectionToggled() reads the button's checked state, which we
    // initialised to m_rightToLeft above, so this is idempotent.
    onDirectionToggled();

    m_clock.start();

    // Decouple sample arrival rate from chart frame rate. At 1280 Hz the
    // raw stream would otherwise force ~1280 replots/sec; we instead
    // accumulate into m_samples and redraw once per frame.
    m_redrawTimer.setInterval(1000 / kRedrawHz);
    connect(&m_redrawTimer, &QTimer::timeout, this, &FastViewDialog::onRedrawTick);
    m_redrawTimer.start();
}

FastViewDialog::~FastViewDialog() = default;

void FastViewDialog::appendSample(double dbm)
{
    if (m_paused) return;       // freeze the captured frame
    const double t = m_clock.elapsed() / 1000.0;
    m_samples.emplace_back(t, dbm);
    // Trim by sample count so the buffer stays at exactly
    // `m_sampleRateHz * m_windowSeconds` entries; rendering uses the
    // sample index (not the timestamp) for X positioning so USB jitter
    // can't squash the trace.
    trimToWindow(t);
}

void FastViewDialog::onRedrawTick()
{
    if (m_paused) return;
    trimToWindow(0.0);
    renderFrame();
}

void FastViewDialog::setSamplingRateHz(int hz)
{
    if (hz <= 0 || hz == m_sampleRateHz) return;
    m_sampleRateHz = hz;
    // Old samples were captured at a different period; mixing them with
    // new ones on an index-domain X axis would produce uneven spacing.
    // Clearest behaviour is to drop them and refill from now.
    m_samples.clear();
    m_clock.restart();
    if (m_plot && m_plot->graph(0)) m_plot->graph(0)->data()->clear();
}

void FastViewDialog::renderFrame()
{
    const int m = static_cast<int>(m_samples.size());
    QVector<double> xs;
    QVector<double> ys;
    xs.reserve(m);
    ys.reserve(m);

    double winMin =  std::numeric_limits<double>::infinity();
    double winMax = -std::numeric_limits<double>::infinity();

    // Index-domain plotting -- one X step per sample, period = 1/rate.
    // Matches the C# reference app (Measure_UserControl.cs:188-190):
    // x[i] = i * (1/sample_rate), regardless of when the sample
    // actually arrived. The trace stays oscilloscope-clean even when
    // USB delivery is bursty.
    const double period = (m_sampleRateHz > 0) ? (1.0 / m_sampleRateHz) : 0.0;

    for (int i = 0; i < m; ++i) {
        const double dbm = m_samples[i].second;
        double x = 0.0;
        if (period > 0.0) {
            if (m_rightToLeft) {
                // Newest sample (i = m-1) anchored at x = windowSeconds;
                // older samples shift left at one period per step.
                x = m_windowSeconds - (m - 1 - i) * period;
            } else {
                // Newest sample (i = m-1) at x = (m-1)*period; oldest at 0.
                x = i * period;
            }
        }
        if (x < 0.0 || x > m_windowSeconds) continue;
        xs.append(x);
        ys.append(dbm);
        winMin = std::min(winMin, dbm);
        winMax = std::max(winMax, dbm);
    }

    // Both directions produce sorted X arrays (monotonic in i), so let
    // QCustomPlot skip the resort.
    m_plot->graph(0)->setData(xs, ys, /*alreadySorted=*/true);
    m_plot->xAxis->setRange(0, m_windowSeconds);
    updateYAxisRange(winMin, winMax);

    refreshStatsLabel();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void FastViewDialog::updateYAxisRange(double windowMin, double windowMax)
{
    if (m_scaleMode == ScaleMode::Manual) {
        // Honour the user's bounds verbatim; never touch them from data.
        m_plot->yAxis->setRange(m_yMinDisplayed, m_yMaxDisplayed);
        return;
    }
    // Auto: track visible window with 5 dB margin, snapped to integer
    // dB. Matches the C# app's auto-scale (Measure_UserControl.cs:201-207)
    // so the grid only moves in whole-dB steps and tightens back down as
    // peaks scroll off.
    if (!std::isfinite(windowMin) || !std::isfinite(windowMax)) return;
    const double newMax = std::floor(windowMax) + 5.0;
    const double newMin = std::floor(windowMin) - 5.0;
    if (newMin == m_yMinDisplayed && newMax == m_yMaxDisplayed) return;
    m_yMinDisplayed = newMin;
    m_yMaxDisplayed = newMax;
    m_plot->yAxis->setRange(m_yMinDisplayed, m_yMaxDisplayed);
}

void FastViewDialog::resetView()
{
    m_samples.clear();
    m_plot->graph(0)->data()->clear();
    m_clock.restart();
    m_yMinDisplayed = -60.0;
    m_yMaxDisplayed = +10.0;
    m_plot->xAxis->setRange(0, m_windowSeconds);
    m_plot->yAxis->setRange(m_yMinDisplayed, m_yMaxDisplayed);
    refreshStatsLabel();
    m_plot->replot();
}

void FastViewDialog::onWindowSecondsChanged(int seconds)
{
    m_windowSeconds = std::max(1, seconds);
    // No live trim here; the next redraw tick takes care of it.
}

void FastViewDialog::onScaleModeChanged(int index)
{
    m_scaleMode = (index == 1) ? ScaleMode::Manual : ScaleMode::Auto;
    const bool manual = (m_scaleMode == ScaleMode::Manual);
    m_yMinSpin->setVisible(manual);
    m_yMaxSpin->setVisible(manual);
    if (manual) {
        // Seed the spinboxes from whatever Auto last computed so the
        // chart doesn't jump when switching modes.
        QSignalBlocker b1(m_yMinSpin);
        QSignalBlocker b2(m_yMaxSpin);
        m_yMinSpin->setValue(m_yMinDisplayed);
        m_yMaxSpin->setValue(m_yMaxDisplayed);
    }
    // Next redraw tick picks up the new mode.
}

void FastViewDialog::onManualYChanged()
{
    if (m_scaleMode != ScaleMode::Manual) return;
    double lo = m_yMinSpin->value();
    double hi = m_yMaxSpin->value();
    if (hi <= lo) hi = lo + 1.0;   // keep range non-degenerate
    m_yMinDisplayed = lo;
    m_yMaxDisplayed = hi;
    m_plot->yAxis->setRange(m_yMinDisplayed, m_yMaxDisplayed);
}

void FastViewDialog::onDirectionToggled()
{
    m_rightToLeft = m_directionButton ? m_directionButton->isChecked() : !m_rightToLeft;
    if (m_directionButton) {
        m_directionButton->setText(m_rightToLeft
                                       ? tr("Flow: \xe2\x86\x90 (newest right)")
                                       : tr("Flow: \xe2\x86\x92 (newest left)"));
    }
    m_plot->xAxis->setLabel(m_rightToLeft ? tr("seconds ago") : tr("seconds since"));
}

void FastViewDialog::onPauseToggled(bool paused)
{
    m_paused = paused;
    m_pauseButton->setText(paused ? tr("Resume") : tr("Pause"));
    // We don't stop the redraw timer here; onRedrawTick early-returns on
    // m_paused. That keeps the slot wiring simple and lets Resume just
    // flip the flag back.
}

QString FastViewDialog::currentStatsText() const
{
    if (m_samples.empty()) {
        return tr("min: -    max: -    avg: -    samples: 0");
    }
    double mn =  std::numeric_limits<double>::infinity();
    double mx = -std::numeric_limits<double>::infinity();
    double sum = 0.0;
    for (const auto &p : m_samples) {
        mn  = std::min(mn, p.second);
        mx  = std::max(mx, p.second);
        sum += p.second;
    }
    const double avg = sum / m_samples.size();
    return tr("min: %1 dBm    max: %2 dBm    avg: %3 dBm    samples: %4")
            .arg(mn, 0, 'f', 2)
            .arg(mx, 0, 'f', 2)
            .arg(avg, 0, 'f', 2)
            .arg(m_samples.size());
}

void FastViewDialog::onSaveScreenshotClicked()
{
    // Render the chart, then paint a footer with the current stats line
    // and a timestamp so the saved image stands on its own.
    const QPixmap plotPm = m_plot->toPixmap();
    if (plotPm.isNull()) return;

    QFont footerFont = font();
    QFontMetrics fm(footerFont);
    const int padding = 8;
    const int lineHeight = fm.height();
    const int footerHeight = padding + 2 * lineHeight + padding;

    QPixmap out(plotPm.width(), plotPm.height() + footerHeight);
    out.fill(Qt::white);
    {
        QPainter p(&out);
        p.drawPixmap(0, 0, plotPm);
        p.setFont(footerFont);
        p.setPen(Qt::black);
        const QString line1 = currentStatsText();
        const QString line2 = tr("captured %1   window %2 s   Y %3..%4 dBm")
                .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
                .arg(m_windowSeconds)
                .arg(m_yMinDisplayed, 0, 'f', 1)
                .arg(m_yMaxDisplayed, 0, 'f', 1);
        const int y1 = plotPm.height() + padding + fm.ascent();
        const int y2 = y1 + lineHeight;
        p.drawText(padding, y1, line1);
        p.drawText(padding, y2, line2);
    }

    // Silent save into the session folder, matching the main chart's
    // saveAllCharts flow. No QFileDialog; user gets a toast pointing at
    // the file. Click the toast to open the containing folder.
    const QString baseDir = m_saveBaseDir.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            : m_saveBaseDir;
    const QString subdir = baseDir + "/saved_charts/" +
            QDateTime::currentDateTime().toString("dd.MM.yyyy_hh.mm.ss.zzz") + "/";
    QDir().mkpath(subdir);
    const QString path = subdir + "fastview.png";
    if (out.save(path, "PNG")) {
        notify::showSavedToast(this, tr("Saved fast view PNG \xe2\x86\x92 %1").arg(path), path);
    } else {
        notify::showSavedToast(this, tr("Save failed: %1").arg(path));
    }
}

void FastViewDialog::onSaveCsvClicked()
{
    if (m_samples.empty()) {
        notify::showSavedToast(this, tr("Nothing to save - buffer is empty"));
        return;
    }

    const QString baseDir = m_saveBaseDir.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            : m_saveBaseDir;
    const QString subdir = baseDir + "/saved_charts/" +
            QDateTime::currentDateTime().toString("dd.MM.yyyy_hh.mm.ss.zzz") + "/";
    QDir().mkpath(subdir);
    const QString path = subdir + "fastview.csv";

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        notify::showSavedToast(this, tr("Save failed: %1").arg(path));
        return;
    }
    QTextStream out(&f);
    // The summary line at the top makes the CSV self-describing without
    // forcing a separate stats file.
    out << "# " << currentStatsText() << "\n";
    out << "# captured " << QDateTime::currentDateTime().toString(Qt::ISODate)
        << "   window " << m_windowSeconds << " s"
        << "   sample_rate " << m_sampleRateHz << " Hz\n";
    out << "sample_index,sample_seconds,arrival_seconds,dbm\n";
    // sample_seconds = index / sample_rate (matches the chart's X axis).
    // arrival_seconds = m_clock-elapsed when the sample reached us, kept
    // so the user can diagnose host-side jitter against the nominal grid.
    const double period = (m_sampleRateHz > 0) ? (1.0 / m_sampleRateHz) : 0.0;
    int i = 0;
    for (const auto &p : m_samples) {
        out << i << ","
            << QString::number(i * period, 'f', 6) << ","
            << QString::number(p.first,    'f', 4) << ","
            << QString::number(p.second,   'f', 4) << "\n";
        ++i;
    }
    f.close();
    notify::showSavedToast(this, tr("Saved fast view CSV \xe2\x86\x92 %1").arg(path), path);
}

void FastViewDialog::trimToWindow(double /*nowSec*/)
{
    // Index-domain plotting: cap the buffer at exactly
    // sample_rate * window_seconds entries. Wall-clock timestamps still
    // live in m_samples for diagnostic / CSV purposes; they don't drive
    // X position any more.
    if (m_sampleRateHz <= 0) return;
    const size_t maxN = static_cast<size_t>(
        std::max(1, m_sampleRateHz * m_windowSeconds));
    while (m_samples.size() > maxN) m_samples.pop_front();
}

void FastViewDialog::refreshStatsLabel()
{
    m_statsLabel->setText(currentStatsText());
}
