// Copyright (c) 2024, RPTU Kaiserslautern-Landau / Fraunhofer IESE
// Premium extension — not for redistribution.

#ifndef LATENCYPLOT_H
#define LATENCYPLOT_H

#include "latencyresult.h"

#include <QList>

class QwtPlot;

namespace TraceAnalyzerExtension
{

/**
 * Renders a READ/WRITE latency histogram with P50/P95/P99 markers
 * on an existing QwtPlot widget.
 *
 * Call renderHistogram() after the LatencyWorker emits resultReady.
 * The function clears any previous items attached to the plot.
 */
void renderHistogram(QwtPlot*                          plot,
                     const QList<LatencyResult>&       results,
                     const LatencyStats&               readStats,
                     const LatencyStats&               writeStats,
                     int                               numBins = 50);

} // namespace TraceAnalyzerExtension

#endif // LATENCYPLOT_H
