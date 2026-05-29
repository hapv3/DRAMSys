

#include "poweranalysis.h"
#include "powerplot.h"
#include "powerworker.h"

#include <QLabel>
#include <QProgressBar>
#include <QThread>
#include <qwt_plot.h>

namespace TraceAnalyzerExtension
{

void powerAnalysis(QSqlDatabase  db,
                   QProgressBar* progressBar,
                   QwtPlot*      powerPlot,
                   QwtPlot*      bwPlot,
                   QwtPlot*      bufferPlot,
                   QLabel*       statsLabel)
{
    qRegisterMetaType<TraceAnalyzerExtension::PowerStats>(
        "TraceAnalyzerExtension::PowerStats");

    // --- Reset UI ---
    if (progressBar)
    {
        progressBar->setValue(0);
        progressBar->setEnabled(true);
    }
    if (statsLabel)
        statsLabel->setText("Analysis in progress…");

    // --- Background worker ---
    auto* thread = new QThread;
    auto* worker = new PowerWorker(db.databaseName());
    worker->moveToThread(thread);

    QObject::connect(thread, &QThread::started,
                     worker, &PowerWorker::run);

    if (progressBar)
        QObject::connect(worker, &PowerWorker::progressUpdated,
                         progressBar, &QProgressBar::setValue);

    // Results → render all three plots + update label
    QObject::connect(worker, &PowerWorker::resultReady,
                     powerPlot,                      // arbitrary receiver on main thread
                     [powerPlot, bwPlot, bufferPlot, statsLabel]
                     (TraceAnalyzerExtension::PowerStats stats)
    {
        renderPowerPlot(powerPlot,  stats.powerSamples,  stats.meanPowerMw);
        renderBandwidthPlot(bwPlot, stats.bwSamples,     stats.meanBwGBs);
        renderBufferPlot(bufferPlot, stats.bufferSamples, stats.numBuffers);

        if (statsLabel)
        {
            QString text;
            if (stats.isValid())
            {
                text = QString(
                    "<b>Power:</b> peak&nbsp;%1&nbsp;mW | "
                    "mean&nbsp;%2&nbsp;mW | "
                    "energy&nbsp;%3&nbsp;nJ"
                    "<br>"
                    "<b>Bandwidth:</b> peak&nbsp;%4&nbsp;GB/s | "
                    "mean&nbsp;%5&nbsp;GB/s"
                    "<br>"
                    "<b>Buffer:</b> %6 buffer(s), max depth&nbsp;%7")
                    .arg(stats.peakPowerMw,   0, 'f', 1)
                    .arg(stats.meanPowerMw,   0, 'f', 1)
                    .arg(stats.totalEnergyNj, 0, 'f', 0)
                    .arg(stats.peakBwGBs,     0, 'f', 3)
                    .arg(stats.meanBwGBs,     0, 'f', 3)
                    .arg(stats.numBuffers)
                    .arg(stats.maxBufferDepth, 0, 'f', 1);
            }
            else
            {
                text = "<i>(No power / bandwidth data found in this trace.)</i>";
            }
            statsLabel->setText(text);
        }
    });

    QObject::connect(worker, &PowerWorker::errorOccurred,
                     powerPlot,
                     [progressBar, statsLabel](const QString& msg)
    {
        if (progressBar) progressBar->setValue(0);
        if (statsLabel)
            statsLabel->setText("<b style='color:red'>Error:</b> " + msg);
    });

    // Cleanup
    QObject::connect(worker, &PowerWorker::finished,
                     thread, &QThread::quit);
    QObject::connect(worker, &PowerWorker::finished,
                     worker, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished,
                     thread, &QObject::deleteLater);

    thread->start();
}

} // namespace TraceAnalyzerExtension
