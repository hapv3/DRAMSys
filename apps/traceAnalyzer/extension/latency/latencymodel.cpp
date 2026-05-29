// Copyright (c) 2024, RPTU Kaiserslautern-Landau / Fraunhofer IESE
// Premium extension — not for redistribution.

#include "latencymodel.h"

#include <QBrush>
#include <QColor>
#include <QFont>

namespace TraceAnalyzerExtension
{

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

LatencyModel::LatencyModel(QObject* parent)
    : QAbstractItemModel(parent),
      m_root(std::make_unique<Node>())
{
    m_root->type = NodeType::Root;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void LatencyModel::setData(const QList<LatencyResult>& results, traceTime clkPeriod)
{
    beginResetModel();
    m_clkPeriod = (clkPeriod > 0) ? clkPeriod : 1;
    buildTree(results);
    endResetModel();
}

// ---------------------------------------------------------------------------
// Tree builder
// ---------------------------------------------------------------------------

void LatencyModel::buildTree(const QList<LatencyResult>& results)
{
    m_root = std::make_unique<Node>();
    m_root->type = NodeType::Root;

    // Create the two top-level group nodes
    auto makeGroup = [&](const QString& name, int row) -> Node*
    {
        auto grp       = std::make_unique<Node>();
        grp->type      = NodeType::Group;
        grp->groupName = name;
        grp->row       = row;
        grp->parent    = m_root.get();
        Node* ptr      = grp.get();
        m_root->children.push_back(std::move(grp));
        return ptr;
    };

    Node* readGroup  = makeGroup("READ",  0);
    Node* writeGroup = makeGroup("WRITE", 1);

    for (const auto& tx : results)
    {
        Node* grpNode = (tx.command == "READ") ? readGroup : writeGroup;
        int   txRow   = static_cast<int>(grpNode->children.size());

        auto txNode      = std::make_unique<Node>();
        txNode->type     = NodeType::Transaction;
        txNode->tx       = tx;
        txNode->clkPeriod = m_clkPeriod;
        txNode->row      = txRow;
        txNode->parent   = grpNode;

        for (const auto& ph : tx.phaseBreakdowns)
        {
            int phRow = static_cast<int>(txNode->children.size());

            auto phNode      = std::make_unique<Node>();
            phNode->type     = NodeType::Phase;
            phNode->phase    = ph;
            phNode->clkPeriod = m_clkPeriod;
            phNode->row      = phRow;
            phNode->parent   = txNode.get();
            txNode->children.push_back(std::move(phNode));
        }

        grpNode->children.push_back(std::move(txNode));
    }
}

// ---------------------------------------------------------------------------
// QAbstractItemModel interface
// ---------------------------------------------------------------------------

LatencyModel::Node* LatencyModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid())
        return m_root.get();
    return static_cast<Node*>(index.internalPointer());
}

QModelIndex LatencyModel::index(int row, int column,
                                const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return {};

    Node* parentNode = nodeFromIndex(parent);
    if (row < 0 || static_cast<size_t>(row) >= parentNode->children.size())
        return {};

    return createIndex(row, column, parentNode->children[static_cast<size_t>(row)].get());
}

QModelIndex LatencyModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return {};

    Node* childNode  = static_cast<Node*>(child.internalPointer());
    Node* parentNode = childNode->parent;

    if (!parentNode || parentNode == m_root.get())
        return {};

    return createIndex(parentNode->row, 0, parentNode);
}

int LatencyModel::rowCount(const QModelIndex& parent) const
{
    Node* node = nodeFromIndex(parent);
    return static_cast<int>(node->children.size());
}

int LatencyModel::columnCount(const QModelIndex& /*parent*/) const
{
    return ColumnCount;
}

QVariant LatencyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    Node* node = static_cast<Node*>(index.internalPointer());

    // ----- Display role -----
    if (role == Qt::DisplayRole)
    {
        if (node->type == NodeType::Group)
        {
            if (index.column() == ColName)
                return QString("%1  (%2 transactions)")
                    .arg(node->groupName)
                    .arg(node->children.size());
            return {};
        }

        if (node->type == NodeType::Transaction)
        {
            const LatencyResult& tx = node->tx;
            switch (index.column())
            {
            case ColName:
                // Leaf ID value: raw txId as uint so on_latencyTreeView_doubleClicked works.
                return tx.transactionId;
            case ColLatNs:
                return QString::number(tx.totalLatencyPs / 1000.0, 'f', 3) + " ns";
            case ColLatClks:
                return QString("%1 clks").arg(tx.totalLatencyCycles);
            case ColAddr:
                return QString("0x%1  R%2-BG%3-B%4")
                    .arg(tx.address, 8, 16, QLatin1Char('0'))
                    .arg(tx.rank).arg(tx.bankGroup).arg(tx.bank);
            default:
                return {};
            }
        }

        if (node->type == NodeType::Phase)
        {
            const PhaseBreakdown& ph = node->phase;
            switch (index.column())
            {
            case ColName:
                return ph.phaseName;
            case ColLatNs:
                return QString::number(ph.durationPs / 1000.0, 'f', 3) + " ns";
            case ColLatClks:
                return QString("%1 clks").arg(ph.durationCycles);
            case ColAddr:
                return {};
            default:
                return {};
            }
        }
    }

    // ----- Foreground (text color) -----
    if (role == Qt::ForegroundRole)
    {
        if (node->type == NodeType::Transaction && node->tx.isOutlier)
            return QBrush(QColor(0xC0392B)); // red for outliers
    }

    // ----- Font -----
    if (role == Qt::FontRole)
    {
        if (node->type == NodeType::Group)
        {
            QFont f;
            f.setBold(true);
            return f;
        }
        if (node->type == NodeType::Transaction && node->tx.isOutlier)
        {
            QFont f;
            f.setBold(true);
            return f;
        }
    }

    // ----- Tooltip -----
    if (role == Qt::ToolTipRole && node->type == NodeType::Transaction)
    {
        const LatencyResult& tx = node->tx;
        QString tip = QString("Transaction ID: %1\n"
                              "Command: %2\n"
                              "Address: 0x%3\n"
                              "Rank: %4  BankGroup: %5  Bank: %6\n"
                              "Total latency: %7 ns  (%8 clks)")
            .arg(tx.transactionId)
            .arg(tx.command)
            .arg(tx.address, 8, 16, QLatin1Char('0'))
            .arg(tx.rank).arg(tx.bankGroup).arg(tx.bank)
            .arg(tx.totalLatencyPs / 1000.0, 0, 'f', 3)
            .arg(tx.totalLatencyCycles);
        if (tx.isOutlier)
            tip += "\n⚠ Outlier (> P99)";
        return tip;
    }

    return {};
}

QVariant LatencyModel::headerData(int section,
                                  Qt::Orientation orientation,
                                  int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section)
    {
    case ColName:    return "ID / Phase";
    case ColLatNs:   return "Latency (ns)";
    case ColLatClks: return "Latency (clks)";
    case ColAddr:    return "Address / Location";
    default:         return {};
    }
}

Qt::ItemFlags LatencyModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// ---------------------------------------------------------------------------
// Sorting
// ---------------------------------------------------------------------------

void LatencyModel::sort(int column, Qt::SortOrder order)
{
    layoutAboutToBeChanged();

    for (auto& group : m_root->children)
    {
        if (group->type == NodeType::Group)
        {
            std::sort(group->children.begin(), group->children.end(),
                [column, order](const std::unique_ptr<Node>& a, const std::unique_ptr<Node>& b) {
                    bool less = false;
                    switch (column)
                    {
                    case ColName:
                        less = a->tx.transactionId < b->tx.transactionId;
                        break;
                    case ColLatNs:
                        less = a->tx.totalLatencyPs < b->tx.totalLatencyPs;
                        break;
                    case ColLatClks:
                        less = a->tx.totalLatencyCycles < b->tx.totalLatencyCycles;
                        break;
                    case ColAddr:
                        less = a->tx.address < b->tx.address;
                        break;
                    default:
                        less = a->tx.transactionId < b->tx.transactionId;
                        break;
                    }
                    return order == Qt::AscendingOrder ? less : !less;
                });

            // Update row indices
            for (size_t i = 0; i < group->children.size(); ++i)
                group->children[i]->row = static_cast<int>(i);
        }
    }

    layoutChanged();
}

} // namespace TraceAnalyzerExtension
