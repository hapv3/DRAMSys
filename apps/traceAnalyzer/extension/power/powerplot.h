

#ifndef POWERPLOT_H
#define POWERPLOT_H

#include "powerresult.h"
#include <vector>

class QwtPlot;

namespace TraceAnalyzerExtension
{

/** Render a time-series power curve (red line + fill) onto the given QwtPlot. */
void renderPowerPlot(QwtPlot* plot,
                     const std::vector<PowerSample>& data,
                     double meanPowerMw);

/** Render a time-series bandwidth curve (blue line + fill) onto the given QwtPlot. */
void renderBandwidthPlot(QwtPlot* plot,
                         const std::vector<BandwidthSample>& data,
                         double meanBwGBs);

/**
 * Render one curve per buffer onto the given QwtPlot.
 * Each buffer gets its own colour; a legend is shown.
 */
void renderBufferPlot(QwtPlot* plot,
                      const std::vector<BufferSample>& data,
                      int numBuffers);

} // namespace TraceAnalyzerExtension

#endif // POWERPLOT_H
