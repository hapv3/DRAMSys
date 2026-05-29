

#ifndef POWERANALYSIS_H
#define POWERANALYSIS_H

#include <QSqlDatabase>

class QProgressBar;
class QwtPlot;
class QLabel;

namespace TraceAnalyzerExtension
{

/**
 * Entry point for the Power, Bandwidth & Buffer Utilization analysis.
 *
 * Matches the adapter signature used by plots.h:
 *   TraceAnalyzerExtension::powerAnalysis(db, pb, powerPlot, bwPlot, bufPlot, label)
 *
 * Non-blocking: launches a background QThread and returns immediately.
 * Results are delivered to the UI thread via Qt's queued connections.
 *
 * @param db           Open QSqlDatabase connection from navigator->TraceFile().getDatabase()
 * @param progressBar  Progress indicator widget (0 → 100); may be nullptr
 * @param powerPlot    QwtPlot widget for the power time-series
 * @param bwPlot       QwtPlot widget for the bandwidth time-series
 * @param bufferPlot   QwtPlot widget for the buffer depth time-series
 * @param statsLabel   Optional QLabel for the textual summary; may be nullptr
 */
void powerAnalysis(QSqlDatabase  db,
                   QProgressBar* progressBar,
                   QwtPlot*      powerPlot,
                   QwtPlot*      bwPlot,
                   QwtPlot*      bufferPlot,
                   QLabel*       statsLabel = nullptr);

} // namespace TraceAnalyzerExtension

#endif // POWERANALYSIS_H
