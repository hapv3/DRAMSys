

#ifndef ADVANCEDMETRICSUI_H
#define ADVANCEDMETRICSUI_H

#include <QWidget>
#include <QList>
#include <QString>
#include <QSqlDatabase>
#include <vector>

class QTreeWidget;
class QTreeWidgetItem;

namespace TraceAnalyzerExtension
{

class AdvancedMetricsUI : public QWidget
{
    Q_OBJECT
public:
    explicit AdvancedMetricsUI(QWidget* parent = nullptr);
    ~AdvancedMetricsUI() override;

    void showAndEvaluateMetrics(const QList<QString>& paths);

private slots:
    void calculateMetrics();

private:
    void setupUI();
    void evaluateForPath(const QString& path, QTreeWidgetItem* parentNode);

    QTreeWidget* m_treeWidget;
    QList<QString> m_currentPaths;
};

} // namespace TraceAnalyzerExtension

#endif // ADVANCEDMETRICSUI_H
