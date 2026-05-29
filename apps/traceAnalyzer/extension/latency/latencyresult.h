

#ifndef LATENCYRESULT_H
#define LATENCYRESULT_H

#include "businessObjects/tracetime.h"

#include <QString>
#include <QMetaType>
#include <QList>
#include <cstdint>
#include <vector>

namespace TraceAnalyzerExtension
{

/**
 * Latency contribution of a single DRAM phase within one transaction.
 */
struct PhaseBreakdown
{
    QString   phaseName;
    traceTime durationPs;     // duration in picoseconds
    traceTime durationCycles; // duration in clock cycles
};

/**
 * Full latency record for one transaction.
 *
 * totalLatency = Ranges.end - Ranges.begin (picoseconds).
 * phaseBreakdowns lists every Phase belonging to this transaction in
 * chronological order, so the caller can render a breakdown tree.
 */
struct LatencyResult
{
    unsigned int transactionId = 0;
    QString      command;          // "READ" or "WRITE"
    uint64_t     address = 0;
    unsigned int rank = 0;
    unsigned int bankGroup = 0;
    unsigned int bank = 0;

    traceTime totalLatencyPs     = 0; // picoseconds
    traceTime totalLatencyCycles = 0; // cycles

    std::vector<PhaseBreakdown> phaseBreakdowns;

    bool isOutlier = false; // set to true when latency > p99 threshold
};

/**
 * Aggregate statistics for a set of LatencyResults.
 * All times are in picoseconds unless the field name says "Cycles".
 */
struct LatencyStats
{
    QString   commandType; // "READ", "WRITE", or "ALL"
    size_t    count = 0;

    traceTime minPs  = 0;
    traceTime maxPs  = 0;
    double    meanPs = 0.0;

    traceTime p50Ps  = 0;
    traceTime p95Ps  = 0;
    traceTime p99Ps  = 0;
    traceTime p999Ps = 0;

    bool isValid() const { return count > 0; }
};

} // namespace TraceAnalyzerExtension

Q_DECLARE_METATYPE(TraceAnalyzerExtension::LatencyResult)
Q_DECLARE_METATYPE(QList<TraceAnalyzerExtension::LatencyResult>)
Q_DECLARE_METATYPE(TraceAnalyzerExtension::LatencyStats)

#endif // LATENCYRESULT_H
