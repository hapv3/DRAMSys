// Copyright (c) 2024, RPTU Kaiserslautern-Landau / Fraunhofer IESE
// Premium extension — not for redistribution.

#include "latencyworker.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace TraceAnalyzerExtension
{

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

LatencyWorker::LatencyWorker(const QString& dbPath,
                             traceTime      clkPeriod,
                             QObject*       parent)
    : QObject(parent),
      m_dbPath(dbPath),
      m_clkPeriod(clkPeriod)
{
}

// ---------------------------------------------------------------------------
// Main entry (called by QThread::started signal)
// ---------------------------------------------------------------------------

void LatencyWorker::run()
{
    // Each thread must have its own QSqlDatabase connection.
    // We use the file path (not the main thread's connection) to avoid
    // the "database does not belong to the calling thread" Qt warning.
    const QString connName =
        QString("latency_worker_%1").arg(reinterpret_cast<quintptr>(this));

    QList<LatencyResult> results;
    LatencyStats         readStats, writeStats;

    // --- Open, query, close — all inside this scope ---
    // The QSqlDatabase object MUST be destroyed before removeDatabase() is called.
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(m_dbPath); // use file path directly — thread-safe
        if (!db.open())
        {
            Q_EMIT errorOccurred(
                QString("LatencyWorker: cannot open database: %1")
                    .arg(db.lastError().text()));
            // db goes out of scope here → destroyed before removeDatabase
        }
        else
        {
            Q_EMIT progressUpdated(5);

            try
            {
                results = fetchAll(db);
            }
            catch (const std::exception& ex)
            {
                Q_EMIT errorOccurred(
                    QString("LatencyWorker SQL error: %1").arg(ex.what()));
                // fall through — results is empty, stats will be invalid
            }

            Q_EMIT progressUpdated(70);

            readStats  = computeStats(results, "READ");
            writeStats = computeStats(results, "WRITE");

            Q_EMIT progressUpdated(85);

            if (readStats.isValid())
                markOutliers(results, readStats.p99Ps);
            if (writeStats.isValid())
                markOutliers(results, writeStats.p99Ps);

            Q_EMIT progressUpdated(95);

            db.close();
        }
    } // ← db object fully destroyed here

    // Now safe to remove the connection with no Qt warnings.
    QSqlDatabase::removeDatabase(connName);

    Q_EMIT progressUpdated(100);
    Q_EMIT resultReady(results, readStats, writeStats);
    Q_EMIT finished();
}

// ---------------------------------------------------------------------------
// SQL fetch
// ---------------------------------------------------------------------------

QList<LatencyResult> LatencyWorker::fetchAll(QSqlDatabase& db)
{
    // One row per (transaction, phase). We group them by transaction ID as we
    // iterate so we don't need a second pass.
    const QString sql =
        "SELECT "
        "  T.ID            AS txID, "
        "  T.Command, "
        "  T.Address, "
        "  (R.end - R.begin) AS totalLatencyPs, "
        "  P.PhaseName, "
        "  (P.PhaseEnd - P.PhaseBegin) AS phaseDurationPs, "
        "  P.Rank, "
        "  P.BankGroup, "
        "  P.Bank "
        "FROM Transactions T "
        "  INNER JOIN Ranges R ON T.Range = R.ID "
        "  INNER JOIN Phases P ON P.Transact = T.ID "
        "ORDER BY T.ID ASC, P.PhaseBegin ASC";

    QSqlQuery query(db);
    if (!query.exec(sql))
        throw std::runtime_error(query.lastError().text().toStdString());

    QList<LatencyResult> results;
    unsigned int         lastTxId = 0;
    bool                 first    = true;

    while (query.next())
    {
        unsigned int txId    = query.value(0).toUInt();
        QString      command = query.value(1).toString().toUpper();
        uint64_t     address = query.value(2).toULongLong();
        traceTime    latPs   = query.value(3).toLongLong();

        QString   phaseName  = query.value(4).toString();
        traceTime phasePs    = query.value(5).toLongLong();
        unsigned int rank    = query.value(6).toUInt();
        unsigned int bgp     = query.value(7).toUInt();
        unsigned int bank    = query.value(8).toUInt();

        if (first || txId != lastTxId)
        {
            first    = false;
            lastTxId = txId;

            LatencyResult r;
            r.transactionId    = txId;
            r.command          = command.startsWith("R") || command.contains("READ") || command.contains("RD")
                                     ? QStringLiteral("READ")
                                     : QStringLiteral("WRITE");
            r.address          = address;
            r.rank             = rank;
            r.bankGroup        = bgp;
            r.bank             = bank;
            r.totalLatencyPs   = latPs;
            r.totalLatencyCycles =
                (m_clkPeriod > 0) ? (latPs / static_cast<traceTime>(m_clkPeriod)) : latPs;
            results.append(r);
        }

        PhaseBreakdown pb;
        pb.phaseName     = phaseName;
        pb.durationPs    = phasePs;
        pb.durationCycles =
            (m_clkPeriod > 0) ? (phasePs / static_cast<traceTime>(m_clkPeriod)) : phasePs;
        results.back().phaseBreakdowns.push_back(pb);
    }

    return results;
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------

LatencyStats LatencyWorker::computeStats(const QList<LatencyResult>& results,
                                         const QString&               cmdFilter)
{
    LatencyStats stats;
    stats.commandType = cmdFilter;

    // Collect latencies for the given command type.
    std::vector<traceTime> lats;
    lats.reserve(static_cast<size_t>(results.size()));
    for (const auto& r : results)
    {
        if (r.command == cmdFilter)
            lats.push_back(r.totalLatencyPs);
    }

    if (lats.empty())
        return stats;

    stats.count = lats.size();
    std::sort(lats.begin(), lats.end());

    stats.minPs  = lats.front();
    stats.maxPs  = lats.back();
    stats.meanPs = static_cast<double>(
                       std::accumulate(lats.begin(), lats.end(), traceTime(0)))
                   / static_cast<double>(stats.count);

    auto percentile = [&](double p) -> traceTime {
        if (lats.size() == 1)
            return lats[0];
        double idx = p * (static_cast<double>(lats.size()) - 1.0);
        size_t lo  = static_cast<size_t>(std::floor(idx));
        size_t hi  = static_cast<size_t>(std::ceil(idx));
        double frac = idx - static_cast<double>(lo);
        return static_cast<traceTime>(
            static_cast<double>(lats[lo]) * (1.0 - frac) +
            static_cast<double>(lats[hi]) * frac);
    };

    stats.p50Ps  = percentile(0.50);
    stats.p95Ps  = percentile(0.95);
    stats.p99Ps  = percentile(0.99);
    stats.p999Ps = percentile(0.999);

    return stats;
}

void LatencyWorker::markOutliers(QList<LatencyResult>& results,
                                 traceTime              threshold)
{
    for (auto& r : results)
    {
        if (r.totalLatencyPs > threshold)
            r.isOutlier = true;
    }
}

} // namespace TraceAnalyzerExtension
