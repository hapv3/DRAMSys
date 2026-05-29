// Copyright (c) 2024, RPTU Kaiserslautern-Landau / Fraunhofer IESE
// Premium extension — not for redistribution.

#include "latencyplot.h"

#include <QColor>
#include <QVector>
#include <algorithm>
#include <cmath>

#include <qwt_plot.h>
#include <qwt_legend.h>
#include <qwt_plot_histogram.h>
#include <qwt_plot_marker.h>
#include <qwt_symbol.h>
#include <qwt_text.h>

namespace TraceAnalyzerExtension
{

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace
{

/** Build equal-width bins over [minVal, maxVal] and count items per bin. */
QVector<QwtIntervalSample> makeBins(const QVector<double>& values,
                                    double                 minVal,
                                    double                 maxVal,
                                    int                    numBins)
{
    QVector<QwtIntervalSample> samples(numBins);
    if (minVal >= maxVal || numBins <= 0)
        return samples;

    double binWidth = (maxVal - minVal) / static_cast<double>(numBins);

    for (int i = 0; i < numBins; ++i)
    {
        double lo           = minVal + i * binWidth;
        double hi           = lo + binWidth;
        samples[i].interval = QwtInterval(lo, hi);
        samples[i].value    = 0.0;
    }

    for (double v : values)
    {
        int bin = static_cast<int>((v - minVal) / binWidth);
        if (bin < 0)            bin = 0;
        if (bin >= numBins)     bin = numBins - 1;
        samples[bin].value += 1.0;
    }

    return samples;
}

/** Add a vertical dashed marker line at xValue with a text label. */
void addMarker(QwtPlot* plot,
               double   xValue,
               const QString& label,
               const QColor&  color)
{
    auto* marker = new QwtPlotMarker;
    marker->setLineStyle(QwtPlotMarker::VLine);
    marker->setLinePen(QPen(color, 1.5, Qt::DashLine));
    marker->setXValue(xValue);

    QwtText txt(label);
    txt.setColor(color);
    QFont f = txt.font();
    f.setPointSize(8);
    f.setBold(true);
    txt.setFont(f);
    marker->setLabel(txt);
    marker->setLabelAlignment(Qt::AlignTop | Qt::AlignRight);

    marker->attach(plot);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void renderHistogram(QwtPlot*                    plot,
                     const QList<LatencyResult>& results,
                     const LatencyStats&         readStats,
                     const LatencyStats&         writeStats,
                     int                         numBins)
{
    if (!plot)
        return;

    // Remove all previously attached items
    plot->detachItems();

    // --- Separate latencies by command type (convert PS → ns for display) ---
    QVector<double> readLats, writeLats;
    double globalMin = std::numeric_limits<double>::max();
    double globalMax = std::numeric_limits<double>::lowest();

    for (const auto& r : results)
    {
        double ns = r.totalLatencyPs / 1000.0;
        globalMin = std::min(globalMin, ns);
        globalMax = std::max(globalMax, ns);

        if (r.command == "READ")
            readLats.append(ns);
        else
            writeLats.append(ns);
    }

    if (globalMin >= globalMax)
        globalMax = globalMin + 1.0;

    // --- Build histograms ---
    auto addHistogram = [&](const QVector<double>& lats,
                            const QString&          title,
                            const QColor&           color)
    {
        if (lats.isEmpty())
            return;

        auto* hist = new QwtPlotHistogram(title);
        hist->setStyle(QwtPlotHistogram::Columns);

        QColor fillColor = color;
        fillColor.setAlpha(160);
        hist->setBrush(QBrush(fillColor));
        hist->setPen(QPen(color.darker(130), 0.5));

        auto bins = makeBins(lats, globalMin, globalMax, numBins);
        hist->setSamples(bins);
        hist->attach(plot);
    };

    addHistogram(readLats,  "READ",  QColor(0x2980B9));  // blue
    addHistogram(writeLats, "WRITE", QColor(0xE67E22));  // orange

    // --- Percentile markers ---
    auto addPercentileMarkers = [&](const LatencyStats& stats, bool /*isRead*/)
    {
        if (!stats.isValid())
            return;
        addMarker(plot, stats.p50Ps  / 1000.0, "P50",   QColor(0x27AE60)); // green
        addMarker(plot, stats.p95Ps  / 1000.0, "P95",   QColor(0xF39C12)); // amber
        addMarker(plot, stats.p99Ps  / 1000.0, "P99",   QColor(0xE74C3C)); // red
    };

    // Prefer READ stats for markers when both exist; otherwise use whatever is valid.
    if (readStats.isValid())
        addPercentileMarkers(readStats, true);
    else if (writeStats.isValid())
        addPercentileMarkers(writeStats, false);

    // --- Axis labels ---
    plot->setAxisTitle(QwtPlot::xBottom, "Latency (ns)");
    plot->setAxisTitle(QwtPlot::yLeft,   "Transaction count");

    // --- Legend ---
    plot->insertLegend(new QwtLegend, QwtPlot::BottomLegend);

    plot->replot();
}

} // namespace TraceAnalyzerExtension
