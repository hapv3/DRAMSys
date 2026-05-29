

#include "powerworker.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace TraceAnalyzerExtension
{

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

PowerWorker::PowerWorker(const QString& dbPath, QObject* parent)
    : QObject(parent), m_dbPath(dbPath)
{
}

// ---------------------------------------------------------------------------
// Main entry (called via QThread::started signal)
// ---------------------------------------------------------------------------

void PowerWorker::run()
{
    const QString connName =
        QString("power_worker_%1").arg(reinterpret_cast<quintptr>(this));

    PowerStats stats;

    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(m_dbPath);
        if (!db.open())
        {
            Q_EMIT errorOccurred(
                QString("PowerWorker: cannot open database: %1")
                    .arg(db.lastError().text()));
        }
        else
        {
            Q_EMIT progressUpdated(5);

            try
            {
                auto power  = fetchPower(db);
                Q_EMIT progressUpdated(35);

                auto bw     = fetchBandwidth(db);
                Q_EMIT progressUpdated(65);

                auto buffer = fetchBuffer(db);
                Q_EMIT progressUpdated(85);

                stats = computeStats(power, bw, buffer);
            }
            catch (const std::exception& ex)
            {
                Q_EMIT errorOccurred(
                    QString("PowerWorker error: %1").arg(ex.what()));
            }

            db.close();
        }
    } // db object destroyed — safe to remove

    QSqlDatabase::removeDatabase(connName);

    Q_EMIT progressUpdated(100);
    Q_EMIT resultReady(stats);
    Q_EMIT finished();
}

// ---------------------------------------------------------------------------
// Fetch helpers
// ---------------------------------------------------------------------------

std::vector<PowerSample> PowerWorker::fetchPower(QSqlDatabase& db)
{
    QSqlQuery q(db);
    if (!q.exec("SELECT time, AveragePower FROM Power ORDER BY time ASC"))
        throw std::runtime_error(q.lastError().text().toStdString());

    std::vector<PowerSample> result;
    while (q.next())
    {
        PowerSample s;
        s.timePs     = q.value(0).toDouble();
        s.avgPowerMw = q.value(1).toDouble();
        result.push_back(s);
    }
    return result;
}

std::vector<BandwidthSample> PowerWorker::fetchBandwidth(QSqlDatabase& db)
{
    QSqlQuery q(db);
    if (!q.exec("SELECT Time, AverageBandwidth FROM Bandwidth ORDER BY Time ASC"))
        throw std::runtime_error(q.lastError().text().toStdString());

    std::vector<BandwidthSample> result;
    while (q.next())
    {
        BandwidthSample s;
        s.timePs   = q.value(0).toDouble();
        s.avgBwGBs = q.value(1).toDouble();
        result.push_back(s);
    }
    return result;
}

std::vector<BufferSample> PowerWorker::fetchBuffer(QSqlDatabase& db)
{
    QSqlQuery q(db);
    if (!q.exec("SELECT Time, BufferNumber, AverageBufferDepth FROM BufferDepth "
                "ORDER BY Time ASC, BufferNumber ASC"))
        throw std::runtime_error(q.lastError().text().toStdString());

    std::vector<BufferSample> result;
    while (q.next())
    {
        BufferSample s;
        s.timePs    = q.value(0).toDouble();
        s.bufferNum = q.value(1).toInt();
        s.avgDepth  = q.value(2).toDouble();
        result.push_back(s);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Aggregate statistics
// ---------------------------------------------------------------------------

PowerStats PowerWorker::computeStats(const std::vector<PowerSample>&     power,
                                     const std::vector<BandwidthSample>& bw,
                                     const std::vector<BufferSample>&    buffer)
{
    PowerStats stats;
    stats.powerSamples  = power;
    stats.bwSamples     = bw;
    stats.bufferSamples = buffer;

    // --- Power ---
    if (!power.empty())
    {
        double sum  = 0.0;
        double peak = 0.0;
        for (const auto& s : power)
        {
            sum  += s.avgPowerMw;
            peak  = std::max(peak, s.avgPowerMw);
        }
        stats.peakPowerMw = peak;
        stats.meanPowerMw = sum / static_cast<double>(power.size());

        // Total energy: mean_power_mW * window_duration_ns = nJ
        // Approximate: use the time span of all samples converted to ns
        if (power.size() >= 2)
        {
            double totalNs = (power.back().timePs - power.front().timePs) / 1000.0;
            stats.totalEnergyNj = stats.meanPowerMw * totalNs;
        }
    }

    // --- Bandwidth ---
    if (!bw.empty())
    {
        double sum  = 0.0;
        double peak = 0.0;
        for (const auto& s : bw)
        {
            sum  += s.avgBwGBs;
            peak  = std::max(peak, s.avgBwGBs);
        }
        stats.peakBwGBs = peak;
        stats.meanBwGBs = sum / static_cast<double>(bw.size());
    }

    // --- Buffer ---
    if (!buffer.empty())
    {
        int    maxNum = 0;
        double maxD   = 0.0;
        for (const auto& s : buffer)
        {
            maxNum = std::max(maxNum, s.bufferNum);
            maxD   = std::max(maxD,   s.avgDepth);
        }
        stats.numBuffers     = maxNum + 1;
        stats.maxBufferDepth = maxD;
    }

    return stats;
}

} // namespace TraceAnalyzerExtension
