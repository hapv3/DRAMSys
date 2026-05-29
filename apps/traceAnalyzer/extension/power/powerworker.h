// Copyright (c) 2024, RPTU Kaiserslautern-Landau / Fraunhofer IESE
// Premium extension — not for redistribution.

#ifndef POWERWORKER_H
#define POWERWORKER_H

#include "powerresult.h"

#include <QObject>
#include <QString>
#include <QSqlDatabase>

namespace TraceAnalyzerExtension
{

/**
 * Background worker that fetches Power, Bandwidth, and BufferDepth data
 * from the trace database and computes aggregate statistics.
 *
 * Must be moved to a QThread before calling run(). Results are delivered
 * via the resultReady() signal on the worker's thread, which Qt
 * automatically queues to the main thread when signals are connected
 * across threads.
 */
class PowerWorker : public QObject
{
    Q_OBJECT

public:
    explicit PowerWorker(const QString& dbPath, QObject* parent = nullptr);

Q_SIGNALS:
    void progressUpdated(int percent);
    void resultReady(TraceAnalyzerExtension::PowerStats stats);
    void errorOccurred(const QString& message);
    void finished();

public Q_SLOTS:
    void run();

private:
    QString m_dbPath;

    std::vector<PowerSample>     fetchPower(QSqlDatabase& db);
    std::vector<BandwidthSample> fetchBandwidth(QSqlDatabase& db);
    std::vector<BufferSample>    fetchBuffer(QSqlDatabase& db);
    PowerStats                   computeStats(const std::vector<PowerSample>&,
                                              const std::vector<BandwidthSample>&,
                                              const std::vector<BufferSample>&);
};

} // namespace TraceAnalyzerExtension

#endif // POWERWORKER_H
