// Copyright (c) 2024, RPTU Kaiserslautern-Landau / Fraunhofer IESE
// Premium extension — not for redistribution.

#ifndef LATENCYANALYSIS_H
#define LATENCYANALYSIS_H

#include "businessObjects/tracetime.h"

#include <QSqlDatabase>

class QProgressBar;
class QTreeView;
class QwtPlot;
class QLabel;

namespace TraceAnalyzerExtension
{

/**
 * Entry point for the Latency Analysis premium feature.
 *
 * Matches the signature expected by the open-source stub in tracefiletab.cpp:
 *   TraceAnalyzerExtension::latencyAnalysis(db, progressBar, treeView, plot);
 *
 * The function is non-blocking: it launches a background QThread and returns
 * immediately. Results are delivered asynchronously via Qt's signal/slot
 * mechanism on the calling (main) thread.
 *
 * @param db             Open QSqlDatabase connection from TraceDB::getDatabase()
 * @param clkPeriod      Clock period in picoseconds from GeneralInfo::clkPeriod
 * @param progressBar    Progress indicator widget (0 → 100)
 * @param treeView       Three-level breakdown tree widget
 * @param histogramPlot  QwtPlot widget for latency histogram
 * @param statsLabel     Optional QLabel to display P50/P95/P99 summary text;
 *                       pass nullptr to skip.
 */
void latencyAnalysis(QSqlDatabase  db,
                     traceTime     clkPeriod,
                     QProgressBar* progressBar,
                     QTreeView*    treeView,
                     QwtPlot*      histogramPlot,
                     QLabel*       statsLabel = nullptr);

} // namespace TraceAnalyzerExtension

#endif // LATENCYANALYSIS_H
