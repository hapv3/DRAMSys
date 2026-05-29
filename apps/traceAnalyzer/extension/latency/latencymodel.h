

#ifndef LATENCYMODEL_H
#define LATENCYMODEL_H

#include "latencyresult.h"

#include <QAbstractItemModel>
#include <QList>
#include <QModelIndex>
#include <QVariant>
#include <memory>
#include <vector>

namespace TraceAnalyzerExtension
{

/**
 * Three-level tree model for the latencyTreeView:
 *
 *   Root (invisible)
 *   ├── READ  (group node, N children)
 *   │   ├── Tx #42  — outlier  [127,000 ps | 127 clks]
 *   │   │   ├── REQ   15,000 ps | 15 clks
 *   │   │   ├── ACT    4,000 ps |  4 clks
 *   │   │   └── RD     4,000 ps |  4 clks
 *   │   └── Tx #55  [45,000 ps | 45 clks]
 *   │       └── ...
 *   └── WRITE (group node, M children)
 *       └── ...
 *
 * Columns:
 *   0 – ID / Name
 *   1 – Latency (ns)
 *   2 – Latency (cycles)
 *   3 – Address / Rank-Bank
 *
 * The leaf "ID / Name" column carries the raw transaction ID as an integer
 * so that on_latencyTreeView_doubleClicked() can call navigator->selectTransaction(id).
 */
class LatencyModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Column
    {
        ColName    = 0,
        ColLatNs   = 1,
        ColLatClks = 2,
        ColAddr    = 3,
        ColumnCount
    };

    // Node types
    enum class NodeType { Root, Group, Transaction, Phase };

    // Tree node — must be fully defined here so .cpp can use all members
    struct Node
    {
        NodeType     type   = NodeType::Root;
        int          row    = 0;   // position within parent’s children list
        Node*        parent = nullptr;

        // Group node
        QString groupName;

        // Transaction node
        LatencyResult tx;

        // Phase node
        PhaseBreakdown phase;
        traceTime      clkPeriod = 1;

        std::vector<std::unique_ptr<Node>> children;
    };

    explicit LatencyModel(QObject* parent = nullptr);

    /** Replace the dataset and reset the view. */
    void setData(const QList<LatencyResult>& results, traceTime clkPeriod);

    // QAbstractItemModel interface
    QModelIndex   index(int row, int column,
                        const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex   parent(const QModelIndex& child) const override;
    int           rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int           columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant      data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant      headerData(int section, Qt::Orientation orientation,
                             int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

private:
    std::unique_ptr<Node> m_root;
    traceTime             m_clkPeriod = 1;

    void buildTree(const QList<LatencyResult>& results);
    Node* nodeFromIndex(const QModelIndex& index) const;
};

} // namespace TraceAnalyzerExtension

#endif // LATENCYMODEL_H
