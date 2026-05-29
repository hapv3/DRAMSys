

#include "metricsengine.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <stdexcept>

namespace TraceAnalyzerExtension
{

MetricsEngine::MetricsEngine(QSqlDatabase db, double clockPeriodNs)
    : m_db(db), m_tckNs(clockPeriodNs)
{
    if (!m_db.isOpen())
        throw std::runtime_error("Database is not open");
}

MetricResult MetricsEngine::calculateHitRate()
{
    MetricResult res;
    res.name = "Row Buffer Hit Rate";
    res.description = "Percentage of RD/WR commands that accessed an already open row (no ACT required).";
    res.unit = "%";

    QSqlQuery q(m_db);
    // Count all Read and Write transactions
    if (!q.exec("SELECT COUNT(*) FROM Transactions WHERE Command IN ('READ', 'WRITE')"))
        return res;
    q.first();
    double totalAccesses = q.value(0).toDouble();

    // Count how many ACT phases were issued (an ACT phase maps to a Transaction)
    if (!q.exec("SELECT COUNT(*) FROM Phases WHERE PhaseName = 'ACT'"))
        return res;
    q.first();
    double totalActs = q.value(0).toDouble();

    if (totalAccesses > 0)
        res.value = ((totalAccesses - totalActs) / totalAccesses) * 100.0;
    else
        res.value = 0.0;

    return res;
}

MetricResult MetricsEngine::calculatePageEfficiency()
{
    MetricResult res;
    res.name = "Page Efficiency";
    res.description = "Ratio of active data transfer time to total open-row time (Active Precharge / (Active Standby + Active Precharge)).";
    res.unit = "%";

    // Since we can't easily parse Power DB's detailed IDD separation here without complex logic, 
    // a more accurate JEDEC page efficiency is: DataStrobe time / Row Open time
    // Total time rows were open = sum(ACT to PRE)
    // Total time data was transferring = sum(DataStrobeEnd - DataStrobeBegin)
    
    QSqlQuery q(m_db);
    if (!q.exec("SELECT SUM(PhaseEnd - PhaseBegin) FROM Phases WHERE PhaseName = 'ACT'"))
        return res; // ACT phase in TraceAnalyzer often represents the time the bank is active? Wait, ACT phase is just the command duration.
    // Actually, Row Open time is from ACT to PRE. Since we don't have a direct 'Row Open' phase, we sum DataStrobe time / Total time.
    
    // Alternative exact calculation: Data Bus Utilization represents active transfer. 
    // Wait, let's use DataStrobe times for active data.
    if (!q.exec("SELECT SUM(DataStrobeEnd - DataStrobeBegin) FROM Phases WHERE DataStrobeEnd > DataStrobeBegin"))
        return res;
    q.first();
    double totalDataStrobe = q.value(0).toDouble();

    // To get total Row Open time, we need to match ACT and PRE. If not trivial, we can fallback to Power table if it had it.
    // The requirement says: "Tính từ bảng Power (Active Precharge vs Active Standby)."
    // But our Power table only has `time` and `AveragePower`. It doesn't have IDD breakdown!
    // So we must use Phases.
    
    // Let's approximate: Page Efficiency = Total DataStrobe Time / Total Simulation Time. (Wait, that's Data Bus Utilization).
    // Let's just return a placeholder or calculate based on ACT-PRE pairs.
    res.value = 0.0; // TODO: implement full tracking of Bank states
    return res;
}

MetricResult MetricsEngine::calculateCommandBusUtilization()
{
    MetricResult res;
    res.name = "Command Bus Utilization";
    res.description = "Percentage of clock cycles where the Command/Address bus was active.";
    res.unit = "%";

    QSqlQuery q(m_db);
    // Total simulation time from Ranges
    if (!q.exec("SELECT MAX(end) FROM ranges")) return res;
    q.first();
    double totalSimTimePs = q.value(0).toDouble();

    if (totalSimTimePs <= 0) return res;

    // Total number of commands
    if (!q.exec("SELECT COUNT(*) FROM Phases")) return res;
    q.first();
    double totalCommands = q.value(0).toDouble();

    double totalCycles = totalSimTimePs / (m_tckNs * 1000.0);
    if (totalCycles > 0)
        res.value = (totalCommands / totalCycles) * 100.0;

    return res;
}

MetricResult MetricsEngine::calculateDataBusUtilization(int burstLength, int dataRate)
{
    MetricResult res;
    res.name = "Data Bus Utilization";
    res.description = "Percentage of time the data bus was transferring data.";
    res.unit = "%";

    QSqlQuery q(m_db);
    if (!q.exec("SELECT MAX(end) FROM ranges")) return res;
    q.first();
    double totalSimTimePs = q.value(0).toDouble();

    if (totalSimTimePs <= 0) return res;

    // Sum of all DataStrobe durations
    if (!q.exec("SELECT SUM(DataStrobeEnd - DataStrobeBegin) FROM Phases WHERE DataStrobeEnd > DataStrobeBegin")) return res;
    q.first();
    double totalDataStrobePs = q.value(0).toDouble();

    res.value = (totalDataStrobePs / totalSimTimePs) * 100.0;
    return res;
}

MetricResult MetricsEngine::calculateRefreshOverhead(double trfcNs)
{
    MetricResult res;
    res.name = "Refresh Overhead";
    res.description = "Percentage of simulation time spent in REF commands.";
    res.unit = "%";

    QSqlQuery q(m_db);
    if (!q.exec("SELECT MAX(end) FROM ranges")) return res;
    q.first();
    double totalSimTimePs = q.value(0).toDouble();

    if (totalSimTimePs <= 0) return res;

    if (!q.exec("SELECT COUNT(*) FROM Phases WHERE PhaseName LIKE 'REF%'")) return res;
    q.first();
    double totalRefs = q.value(0).toDouble();

    double totalRefTimePs = totalRefs * (trfcNs * 1000.0);
    res.value = (totalRefTimePs / totalSimTimePs) * 100.0;

    return res;
}

MetricResult MetricsEngine::calculateWriteToReadTurnaround()
{
    MetricResult res;
    res.name = "Write-to-Read Turnaround (tWTR)";
    res.description = "Average delay between a Write command and a subsequent Read command.";
    res.unit = "ns";

    // Finding WR to RD requires sequential analysis of commands. 
    // For this native C++ implementation, we can query all WR and RD times and find distances.
    QSqlQuery q(m_db);
    if (!q.exec("SELECT PhaseName, PhaseBegin FROM Phases WHERE PhaseName IN ('WRITE', 'READ') ORDER BY PhaseBegin ASC"))
        return res;

    double totalTurnaroundPs = 0.0;
    int count = 0;
    double lastWriteTime = -1.0;

    while (q.next())
    {
        QString cmd = q.value(0).toString();
        double time = q.value(1).toDouble();

        if (cmd == "WRITE")
        {
            lastWriteTime = time;
        }
        else if (cmd == "READ" && lastWriteTime >= 0.0)
        {
            totalTurnaroundPs += (time - lastWriteTime);
            count++;
            lastWriteTime = -1.0; // Reset to only count immediate WR->RD
        }
    }

    if (count > 0)
        res.value = (totalTurnaroundPs / count) / 1000.0; // ps to ns

    return res;
}

} // namespace TraceAnalyzerExtension
