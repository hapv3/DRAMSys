// Copyright (c) 2024, RPTU Kaiserslautern-Landau / Fraunhofer IESE
// Premium extension public API — not for redistribution.
//
// This header is the single include used by the open-source code:
//   #ifdef EXTENSION_ENABLED
//   #include <plots.h>
//   #endif

#ifndef PLOTS_H
#define PLOTS_H

#include "latency/latencyanalysis.h"
#include "power/poweranalysis.h"
#include "metrics/advancedmetricsui.h"
#include "businessObjects/tracetime.h"

#include <QLabel>
#include <QProgressBar>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTreeView>
#include <QWidget>

class QwtPlot;

namespace TraceAnalyzerExtension
{

/**
 * Adapter for advanced metrics UI
 */
inline void showAdvancedMetrics(const QList<QString>& paths)
{
    // Simple singleton to keep the window alive
    static AdvancedMetricsUI* ui = nullptr;
    if (!ui) {
        ui = new AdvancedMetricsUI();
    }
    ui->showAndEvaluateMetrics(paths);
}

/**
 * Adapter for latencyAnalysis: reads clkPeriod from GeneralInfo and
 * finds the latencyStatsLabel sibling widget automatically.
 */
inline void latencyAnalysis(QSqlDatabase  db,
                            QProgressBar* progressBar,
                            QTreeView*    treeView,
                            QwtPlot*      histogramPlot)
{
    traceTime clkPeriod = 1000; // default: 1 ns
    {
        QSqlQuery q(db);
        if (q.exec("SELECT clk FROM GeneralInfo") && q.first())
            clkPeriod = static_cast<traceTime>(q.value(0).toLongLong());
    }

    QLabel* statsLabel = nullptr;
    if (treeView && treeView->parentWidget())
        statsLabel = treeView->parentWidget()->findChild<QLabel*>("latencyStatsLabel");

    latencyAnalysis(db, clkPeriod, progressBar, treeView, histogramPlot, statsLabel);
}

/**
 * Adapter for powerAnalysis: finds progressBar and statsLabel from
 * the widget hierarchy of the Power tab automatically.
 */
inline void powerAnalysis(QSqlDatabase db,
                          QwtPlot*     powerPlot,
                          QwtPlot*     bandwidthPlot,
                          QwtPlot*     bufferPlot)
{
    QProgressBar* pb    = nullptr;
    QLabel*       label = nullptr;

    // Walk up to the tab widget to find siblings
    QWidget* tab = powerPlot ? powerPlot->parentWidget() : nullptr;
    while (tab)
    {
        pb    = tab->findChild<QProgressBar*>("powerAnalysisProgressBar");
        label = tab->findChild<QLabel*>("powerStatsLabel");
        if (pb || label) break;
        tab = tab->parentWidget();
    }

    powerAnalysis(db, pb, powerPlot, bandwidthPlot, bufferPlot, label);
}

} // namespace TraceAnalyzerExtension

#endif // PLOTS_H
