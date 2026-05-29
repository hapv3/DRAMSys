

#include "advancedmetricsui.h"
#include "metricsengine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSqlDatabase>
#include <QFileInfo>
#include <QMessageBox>

namespace TraceAnalyzerExtension
{

AdvancedMetricsUI::AdvancedMetricsUI(QWidget* parent) : QWidget(parent)
{
    setupUI();
}

AdvancedMetricsUI::~AdvancedMetricsUI() = default;

void AdvancedMetricsUI::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    
    auto* btnLayout = new QHBoxLayout();
    auto* calcBtn = new QPushButton("Calculate Premium Metrics (Native C++)", this);
    connect(calcBtn, &QPushButton::clicked, this, &AdvancedMetricsUI::calculateMetrics);
    
    btnLayout->addStretch();
    btnLayout->addWidget(calcBtn);
    layout->addLayout(btnLayout);

    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabels({"Metric", "Value", "Unit", "Description"});
    m_treeWidget->setAlternatingRowColors(true);
    layout->addWidget(m_treeWidget);
    
    setWindowTitle("Advanced Metrics Framework");
    resize(800, 600);
}

void AdvancedMetricsUI::showAndEvaluateMetrics(const QList<QString>& paths)
{
    m_currentPaths = paths;
    m_treeWidget->clear();
    show();
    raise();
    activateWindow();
    
    // Auto-calculate on open for convenience
    calculateMetrics();
}

void AdvancedMetricsUI::calculateMetrics()
{
    m_treeWidget->clear();
    
    for (const QString& path : m_currentPaths)
    {
        auto* fileNode = new QTreeWidgetItem(m_treeWidget);
        fileNode->setText(0, QFileInfo(path).baseName());
        
        evaluateForPath(path, fileNode);
        fileNode->setExpanded(true);
    }
}

void AdvancedMetricsUI::evaluateForPath(const QString& path, QTreeWidgetItem* parentNode)
{
    QString connName = "AdvancedMetricsDB_" + path;
    QSqlDatabase db;
    if (QSqlDatabase::contains(connName)) {
        db = QSqlDatabase::database(connName);
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(path);
        if (!db.open()) {
            QMessageBox::warning(this, "Error", "Could not open trace database: " + path);
            return;
        }
    }

    try {
        MetricsEngine engine(db, 1.0); // Assume 1.0 ns tCK if not extracted from GeneralInfo
        
        std::vector<MetricResult> results;
        results.push_back(engine.calculateHitRate());
        // results.push_back(engine.calculatePageEfficiency()); // WIP
        results.push_back(engine.calculateCommandBusUtilization());
        results.push_back(engine.calculateDataBusUtilization(8, 2)); // Assuming BL8 DDR
        results.push_back(engine.calculateRefreshOverhead(350.0)); // Assuming 350ns tRFC
        results.push_back(engine.calculateWriteToReadTurnaround());

        for (const auto& res : results)
        {
            auto* item = new QTreeWidgetItem(parentNode);
            item->setText(0, res.name);
            item->setText(1, QString::number(res.value, 'f', 2));
            item->setText(2, res.unit);
            item->setText(3, res.description);
        }
    }
    catch (const std::exception& e) {
        auto* item = new QTreeWidgetItem(parentNode);
        item->setText(0, "Error");
        item->setText(1, e.what());
    }
}

} // namespace TraceAnalyzerExtension
