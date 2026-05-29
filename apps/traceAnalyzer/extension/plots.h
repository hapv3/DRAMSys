// Copyright (c) 2024, RPTU Kaiserslautern-Landau / Fraunhofer IESE
// Premium extension public API — not for redistribution.
//
// This header is the single include used by the open-source code:
//   #ifdef EXTENSION_ENABLED
//   #include <plots.h>
//   #endif

#ifndef PLOTS_H
#define PLOTS_H

#include "latency/latencyanalysis.h"
#include "businessObjects/tracetime.h"

#include <QLabel>
#include <QProgressBar>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTreeView>

// ---------------------------------------------------------------------------
// Forward declarations for future extensions (power, bandwidth, buffer)
// ---------------------------------------------------------------------------
class QProgressBar;
class QTreeView;
class QwtPlot;
class QLabel;

namespace TraceAnalyzerExtension
{

/**
 * Adapter matching the original open-source stub signature:
 *
 *   TraceAnalyzerExtension::latencyAnalysis(db, progressBar, treeView, plot)
 *
 * The adapter reads clkPeriod from GeneralInfo stored in the database and
 * looks for a QLabel named "latencyStatsLabel" as a sibling of treeView
 * to display the percentile summary.
 */
inline void latencyAnalysis(QSqlDatabase  db,
                            QProgressBar* progressBar,
                            QTreeView*    treeView,
                            QwtPlot*      histogramPlot)
{
    // Read clock period from GeneralInfo table.
    traceTime clkPeriod = 1000; // default: 1 ns
    {
        QSqlQuery q(db);
        if (q.exec("SELECT Clk FROM GeneralInfo") && q.first())
            clkPeriod = static_cast<traceTime>(q.value(0).toLongLong());
    }

    // Locate the optional stats label: look for a QLabel sibling widget.
    QLabel* statsLabel = nullptr;
    if (treeView && treeView->parentWidget())
    {
        statsLabel = treeView->parentWidget()->findChild<QLabel*>("latencyStatsLabel");
    }

    latencyAnalysis(db, clkPeriod, progressBar, treeView, histogramPlot, statsLabel);
}

/**
 * Power / Bandwidth / Buffer analysis — stub for future implementation.
 */
inline void powerAnalysis(QSqlDatabase /*db*/,
                          QwtPlot*     /*powerPlot*/,
                          QwtPlot*     /*bandwidthPlot*/,
                          QwtPlot*     /*bufferPlot*/)
{
    // TODO: implement in Feature 1.2
}

} // namespace TraceAnalyzerExtension

#endif // PLOTS_H
