// Copyright (c) 2024, RPTU Kaiserslautern-Landau / Fraunhofer IESE
// Premium extension — not for redistribution.

#ifndef POWERRESULT_H
#define POWERRESULT_H

#include <QMetaType>
#include <QList>
#include <vector>

namespace TraceAnalyzerExtension
{

/** One sample from the Power table. */
struct PowerSample
{
    double timePs     = 0.0; ///< Window end time in picoseconds
    double avgPowerMw = 0.0; ///< Average power in mW
};

/** One sample from the Bandwidth table. */
struct BandwidthSample
{
    double timePs      = 0.0; ///< Window end time in picoseconds
    double avgBwGBs    = 0.0; ///< Average bandwidth in GB/s
};

/** One sample from the BufferDepth table. */
struct BufferSample
{
    double timePs    = 0.0;
    int    bufferNum = 0;
    double avgDepth  = 0.0;
};

/**
 * All results produced by PowerWorker.
 */
struct PowerStats
{
    // Power
    double peakPowerMw   = 0.0;
    double meanPowerMw   = 0.0;
    double totalEnergyNj = 0.0; ///< mW * ns = nJ

    // Bandwidth
    double peakBwGBs = 0.0;
    double meanBwGBs = 0.0;

    // Buffer
    double maxBufferDepth = 0.0;
    int    numBuffers     = 0;

    // Raw series (for plotting)
    std::vector<PowerSample>     powerSamples;
    std::vector<BandwidthSample> bwSamples;
    std::vector<BufferSample>    bufferSamples;

    bool isValid() const { return !powerSamples.empty() || !bwSamples.empty(); }
};

} // namespace TraceAnalyzerExtension

Q_DECLARE_METATYPE(TraceAnalyzerExtension::PowerStats)

#endif // POWERRESULT_H
