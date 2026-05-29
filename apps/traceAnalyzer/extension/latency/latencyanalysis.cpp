

#include "latencyanalysis.h"
#include "latencymodel.h"
#include "latencyplot.h"
#include "latencyworker.h"

#include <QLabel>
#include <QProgressBar>
#include <QThread>
#include <QTreeView>
#include <qwt_plot.h>

namespace TraceAnalyzerExtension
{

void latencyAnalysis(QSqlDatabase  db,
                     traceTime     clkPeriod,
                     QProgressBar* progressBar,
                     QTreeView*    treeView,
                     QwtPlot*      histogramPlot,
                     QLabel*       statsLabel)
{
    qRegisterMetaType<TraceAnalyzerExtension::LatencyResult>("TraceAnalyzerExtension::LatencyResult");
    qRegisterMetaType<QList<TraceAnalyzerExtension::LatencyResult>>("QList<TraceAnalyzerExtension::LatencyResult>");
    qRegisterMetaType<TraceAnalyzerExtension::LatencyStats>("TraceAnalyzerExtension::LatencyStats");

    // --- Reset UI ---
    if (progressBar)
    {
        progressBar->setValue(0);
        progressBar->setEnabled(true);
    }
    if (statsLabel)
        statsLabel->setText("Analysis in progress…");

    // The model needs to outlive this function call; parent it to the treeView
    // so it is cleaned up automatically when the tab closes.
    auto* model = new LatencyModel(treeView);

    // --- Wire up background worker ---
    auto* thread = new QThread;
    auto* worker = new LatencyWorker(db.databaseName(), clkPeriod);
    worker->moveToThread(thread);

    // Worker signals → main thread slots (QueuedConnection is implicit here
    // because sender and receiver are on different threads).
    QObject::connect(thread, &QThread::started,
                     worker, &LatencyWorker::run);

    QObject::connect(worker, &LatencyWorker::progressUpdated,
                     progressBar, &QProgressBar::setValue);

    QObject::connect(worker, &LatencyWorker::resultReady,
                     treeView,
                     [model, treeView, histogramPlot, statsLabel, clkPeriod]
                     (QList<LatencyResult>  results,
                      LatencyStats          readStats,
                      LatencyStats          writeStats)
    {
        // --- Populate tree ---
        model->setData(results, clkPeriod);
        treeView->setModel(model);
        treeView->expandToDepth(0); // expand group nodes only
        for (int c = 0; c < model->columnCount(); ++c)
            treeView->resizeColumnToContents(c);

        // --- Render histogram ---
        if (histogramPlot)
            renderHistogram(histogramPlot, results, readStats, writeStats);

        // --- Update stats label ---
        if (statsLabel)
        {
            auto fmtStats = [](const LatencyStats& s) -> QString
            {
                if (!s.isValid())
                    return QString("(%1: no data)").arg(s.commandType);
                return QString(
                    "<b>%1</b>  count=%2 | "
                    "min=%3 ns | mean=%4 ns | "
                    "P50=%5 ns | P95=%6 ns | P99=%7 ns | max=%8 ns")
                    .arg(s.commandType)
                    .arg(s.count)
                    .arg(s.minPs  / 1000.0, 0, 'f', 1)
                    .arg(s.meanPs / 1000.0, 0, 'f', 1)
                    .arg(s.p50Ps  / 1000.0, 0, 'f', 1)
                    .arg(s.p95Ps  / 1000.0, 0, 'f', 1)
                    .arg(s.p99Ps  / 1000.0, 0, 'f', 1)
                    .arg(s.maxPs  / 1000.0, 0, 'f', 1);
            };

            statsLabel->setText(fmtStats(readStats) + "<br>" + fmtStats(writeStats));
        }
    });

    QObject::connect(worker, &LatencyWorker::errorOccurred,
                     treeView,
                     [progressBar, statsLabel](const QString& msg)
    {
        if (progressBar) progressBar->setValue(0);
        if (statsLabel)  statsLabel->setText("<b style='color:red'>Error:</b> " + msg);
    });

    // Cleanup: worker and thread delete themselves after work is done.
    QObject::connect(worker, &LatencyWorker::finished,
                     thread, &QThread::quit);
    QObject::connect(worker, &LatencyWorker::finished,
                     worker, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished,
                     thread, &QObject::deleteLater);

    thread->start();
}

} // namespace TraceAnalyzerExtension
