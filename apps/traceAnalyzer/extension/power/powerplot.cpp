// Copyright (c) 2024, RPTU Kaiserslautern-Landau / Fraunhofer IESE
// Premium extension — not for redistribution.

#include "powerplot.h"

#include <QColor>
#include <QPen>
#include <QVector>
#include <QFont>

#include <qwt_plot.h>
#include <qwt_legend.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_marker.h>
#include <qwt_text.h>
#include <qwt_symbol.h>

namespace TraceAnalyzerExtension
{

namespace
{

/** Add a horizontal dashed mean-line at yValue. */
void addHMeanLine(QwtPlot* plot, double yValue, const QColor& color, const QString& label)
{
    auto* marker = new QwtPlotMarker;
    marker->setLineStyle(QwtPlotMarker::HLine);
    marker->setLinePen(QPen(color, 1.2, Qt::DashLine));
    marker->setYValue(yValue);

    QwtText txt(label);
    txt.setColor(color);
    QFont f = txt.font();
    f.setPointSize(8);
    txt.setFont(f);
    marker->setLabel(txt);
    marker->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    marker->attach(plot);
}

/** Palette of distinct colours for multi-buffer rendering. */
QColor bufferColor(int idx)
{
    static const QColor palette[] = {
        QColor(0x2ECC71), // green
        QColor(0x3498DB), // blue
        QColor(0xE67E22), // orange
        QColor(0x9B59B6), // purple
        QColor(0xE74C3C), // red
        QColor(0x1ABC9C), // teal
        QColor(0xF39C12), // amber
        QColor(0x34495E), // dark
    };
    return palette[idx % 8];
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Power plot
// ---------------------------------------------------------------------------

void renderPowerPlot(QwtPlot* plot,
                     const std::vector<PowerSample>& data,
                     double meanPowerMw)
{
    if (!plot) return;
    plot->detachItems();

    if (data.empty())
    {
        plot->replot();
        return;
    }

    QVector<double> xs, ys;
    xs.reserve(static_cast<int>(data.size()));
    ys.reserve(static_cast<int>(data.size()));

    for (const auto& s : data)
    {
        xs.append(s.timePs / 1000.0); // ps → ns
        ys.append(s.avgPowerMw);
    }

    // Main curve — red fill
    auto* curve = new QwtPlotCurve("Power");
    curve->setSamples(xs, ys);

    QColor lineColor(0xE74C3C); // red
    curve->setPen(QPen(lineColor, 1.5));
    QColor fillColor = lineColor;
    fillColor.setAlpha(50);
    curve->setBrush(QBrush(fillColor));
    curve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    curve->attach(plot);

    // Mean line
    if (meanPowerMw > 0.0)
        addHMeanLine(plot, meanPowerMw, lineColor.darker(140),
                     QString("mean %1 mW").arg(meanPowerMw, 0, 'f', 1));

    plot->setAxisTitle(QwtPlot::xBottom, "Time (ns)");
    plot->setAxisTitle(QwtPlot::yLeft,   "Power (mW)");
    plot->insertLegend(new QwtLegend, QwtPlot::BottomLegend);
    plot->replot();
}

// ---------------------------------------------------------------------------
// Bandwidth plot
// ---------------------------------------------------------------------------

void renderBandwidthPlot(QwtPlot* plot,
                         const std::vector<BandwidthSample>& data,
                         double meanBwGBs)
{
    if (!plot) return;
    plot->detachItems();

    if (data.empty())
    {
        plot->replot();
        return;
    }

    QVector<double> xs, ys;
    xs.reserve(static_cast<int>(data.size()));
    ys.reserve(static_cast<int>(data.size()));

    for (const auto& s : data)
    {
        xs.append(s.timePs / 1000.0); // ps → ns
        ys.append(s.avgBwGBs);
    }

    auto* curve = new QwtPlotCurve("Bandwidth");
    curve->setSamples(xs, ys);

    QColor lineColor(0x2980B9); // blue
    curve->setPen(QPen(lineColor, 1.5));
    QColor fillColor = lineColor;
    fillColor.setAlpha(50);
    curve->setBrush(QBrush(fillColor));
    curve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    curve->attach(plot);

    if (meanBwGBs > 0.0)
        addHMeanLine(plot, meanBwGBs, lineColor.darker(140),
                     QString("mean %1 GB/s").arg(meanBwGBs, 0, 'f', 3));

    plot->setAxisTitle(QwtPlot::xBottom, "Time (ns)");
    plot->setAxisTitle(QwtPlot::yLeft,   "Bandwidth (GB/s)");
    plot->insertLegend(new QwtLegend, QwtPlot::BottomLegend);
    plot->replot();
}

// ---------------------------------------------------------------------------
// Buffer depth plot
// ---------------------------------------------------------------------------

void renderBufferPlot(QwtPlot* plot,
                      const std::vector<BufferSample>& data,
                      int numBuffers)
{
    if (!plot) return;
    plot->detachItems();

    if (data.empty() || numBuffers <= 0)
    {
        plot->replot();
        return;
    }

    // Split samples per buffer
    std::vector<QVector<double>> xPerBuf(numBuffers);
    std::vector<QVector<double>> yPerBuf(numBuffers);

    for (const auto& s : data)
    {
        if (s.bufferNum >= 0 && s.bufferNum < numBuffers)
        {
            xPerBuf[s.bufferNum].append(s.timePs / 1000.0); // ps → ns
            yPerBuf[s.bufferNum].append(s.avgDepth);
        }
    }

    for (int i = 0; i < numBuffers; ++i)
    {
        if (xPerBuf[i].isEmpty()) continue;

        auto* curve = new QwtPlotCurve(QString("Buffer %1").arg(i));
        curve->setSamples(xPerBuf[i], yPerBuf[i]);

        QColor c = bufferColor(i);
        curve->setPen(QPen(c, 1.5));
        curve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
        curve->attach(plot);
    }

    plot->setAxisTitle(QwtPlot::xBottom, "Time (ns)");
    plot->setAxisTitle(QwtPlot::yLeft,   "Buffer Depth");
    plot->insertLegend(new QwtLegend, QwtPlot::BottomLegend);
    plot->replot();
}

} // namespace TraceAnalyzerExtension
