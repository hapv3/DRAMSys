

#ifndef METRICSENGINE_H
#define METRICSENGINE_H

#include <QSqlDatabase>
#include <QString>
#include <vector>

namespace TraceAnalyzerExtension
{

/**
 * Common struct for predefined metrics.
 */
struct MetricResult
{
    QString name;
    QString description;
    double  value = 0.0;
    QString unit;
    
    // Time-series if windowing is requested
    std::vector<double> timeWindows;
    std::vector<double> windowValues;
};

/**
 * Calculates advanced DRAM metrics directly from the SQLite trace database,
 * bypassing external python scripts. Implements JEDEC-based formulas.
 */
class MetricsEngine
{
public:
    explicit MetricsEngine(QSqlDatabase db, double clockPeriodNs = 1.0);

    // --- Predefined JEDEC-based Metrics ---
    
    /**
     * Hit Rate: (Total RD/WR - Total ACT) / Total RD/WR
     * Measures how often a row buffer was already open for access.
     */
    MetricResult calculateHitRate();

    /**
     * Page Efficiency: Active Precharge Time / Total Active Time (Standby + Precharge)
     * Derived from Power database if available.
     */
    MetricResult calculatePageEfficiency();

    /**
     * Command Bus Utilization: Commands / (Total Time / tCK)
     */
    MetricResult calculateCommandBusUtilization();

    /**
     * Data Bus Utilization: (RD+WR * BL) / (Total Time * Data Rate)
     */
    MetricResult calculateDataBusUtilization(int burstLength, int dataRate);

    /**
     * Refresh Overhead: (REF commands * tRFC) / Total Time
     */
    MetricResult calculateRefreshOverhead(double trfcNs);

    /**
     * Write-to-Read Turnaround: Average time between a Write and subsequent Read.
     */
    MetricResult calculateWriteToReadTurnaround();

private:
    QSqlDatabase m_db;
    double       m_tckNs; // Clock period in ns
};

} // namespace TraceAnalyzerExtension

#endif // METRICSENGINE_H
