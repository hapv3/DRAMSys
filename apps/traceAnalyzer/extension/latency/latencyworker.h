

#ifndef LATENCYWORKER_H
#define LATENCYWORKER_H

#include "latencyresult.h"

#include <QList>
#include <QObject>
#include <QSqlDatabase>

namespace TraceAnalyzerExtension
{

/**
 * Background worker that fetches all transactions from the SQLite database,
 * computes per-transaction latencies and aggregate statistics, then emits
 * resultReady when done.
 *
 * Run this in a dedicated QThread to avoid blocking the UI.
 *
 * Usage:
 *   auto* thread = new QThread;
 *   auto* worker = new LatencyWorker(db, clkPeriod);
 *   worker->moveToThread(thread);
 *   connect(thread, &QThread::started, worker, &LatencyWorker::run);
 *   connect(worker, &LatencyWorker::resultReady, ..., Qt::QueuedConnection);
 *   connect(worker, &LatencyWorker::finished, thread, &QThread::quit);
 *   connect(worker, &LatencyWorker::finished, worker, &QObject::deleteLater);
 *   connect(thread, &QThread::finished, thread, &QObject::deleteLater);
 *   thread->start();
 */
class LatencyWorker : public QObject
{
    Q_OBJECT

public:
    explicit LatencyWorker(const QString& dbPath,
                           traceTime      clkPeriod,
                           QObject*       parent = nullptr);

Q_SIGNALS:
    void progressUpdated(int percent);
    void resultReady(QList<TraceAnalyzerExtension::LatencyResult> results,
                     TraceAnalyzerExtension::LatencyStats          readStats,
                     TraceAnalyzerExtension::LatencyStats          writeStats);
    void errorOccurred(QString message);
    void finished();

public Q_SLOTS:
    void run();

private:
    QString   m_dbPath;      // SQLite file path — safe to use from any thread
    traceTime m_clkPeriod;

    QList<LatencyResult> fetchAll(QSqlDatabase& db);
    LatencyStats         computeStats(const QList<LatencyResult>& results,
                                      const QString&               cmdFilter);
    void                 markOutliers(QList<LatencyResult>& results,
                                      traceTime              threshold);
};

} // namespace TraceAnalyzerExtension

#endif // LATENCYWORKER_H
