

#include <gtest/gtest.h>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QString>
#include <QVariant>
#include <iostream>

// Include the class to be tested
#include "../extension/metrics/metricsengine.h"

using namespace TraceAnalyzerExtension;

/**
 * Mocks the SQLite database structure required by TraceAnalyzer.
 */
class MetricsEngineTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Setup in-memory database
        db = QSqlDatabase::addDatabase("QSQLITE", "test_metrics");
        db.setDatabaseName(":memory:");
        ASSERT_TRUE(db.open()) << "Failed to open in-memory database";

        QSqlQuery q(db);
        // Create necessary tables
        ASSERT_TRUE(q.exec("CREATE TABLE Transactions(ID INTEGER, Command TEXT)"));
        ASSERT_TRUE(q.exec("CREATE TABLE Phases(PhaseName TEXT, PhaseBegin INTEGER, PhaseEnd INTEGER, DataStrobeBegin INTEGER, DataStrobeEnd INTEGER)"));
        ASSERT_TRUE(q.exec("CREATE TABLE ranges(id INTEGER, begin INTEGER, end INTEGER)"));
        ASSERT_TRUE(q.exec("CREATE TABLE Power(time DOUBLE, AveragePower DOUBLE)"));
    }

    void TearDown() override
    {
        db.close();
        QSqlDatabase::removeDatabase("test_metrics");
    }

    // Helper to insert a transaction
    void insertTransaction(int id, const QString& cmd)
    {
        QSqlQuery q(db);
        q.prepare("INSERT INTO Transactions (ID, Command) VALUES (?, ?)");
        q.addBindValue(id);
        q.addBindValue(cmd);
        q.exec();
    }

    // Helper to insert a phase
    void insertPhase(const QString& name, double begin, double end, double dsBegin = 0.0, double dsEnd = 0.0)
    {
        QSqlQuery q(db);
        q.prepare("INSERT INTO Phases (PhaseName, PhaseBegin, PhaseEnd, DataStrobeBegin, DataStrobeEnd) VALUES (?, ?, ?, ?, ?)");
        q.addBindValue(name);
        q.addBindValue(begin);
        q.addBindValue(end);
        q.addBindValue(dsBegin);
        q.addBindValue(dsEnd);
        q.exec();
    }

    // Helper to set total simulation time
    void setSimulationTime(double maxEndPs)
    {
        QSqlQuery q(db);
        q.prepare("INSERT INTO ranges (id, begin, end) VALUES (1, 0, ?)");
        q.addBindValue(maxEndPs);
        q.exec();
    }

    QSqlDatabase db;
};

// -------------------------------------------------------------------------
// DDR4 JEDEC Test Cases
// -------------------------------------------------------------------------

TEST_F(MetricsEngineTest, DDR4_HitRate)
{
    // Simulate 100 Transactions (READ/WRITE)
    for (int i = 0; i < 100; i++)
        insertTransaction(i, "READ");
    
    // Simulate 40 ACT commands (meaning 60 hits)
    for (int i = 0; i < 40; i++)
        insertPhase("ACT", i*10, i*10+5);

    MetricsEngine engine(db, 1.0);
    MetricResult res = engine.calculateHitRate();
    
    // Hit Rate should be (100 - 40)/100 = 60.0%
    ASSERT_NEAR(res.value, 60.0, 0.01);
}

TEST_F(MetricsEngineTest, DDR4_DataBusUtilization)
{
    // Simulate 1000 ns total time (1,000,000 ps)
    setSimulationTime(1000000.0);

    // Simulate 10 bursts of DataStrobe, each taking 10 ns (10,000 ps)
    for (int i = 0; i < 10; i++)
        insertPhase("DATA", 0, 0, i*20000, i*20000 + 10000);

    // Total data time = 10 * 10,000 = 100,000 ps
    // Utilization = 100,000 / 1,000,000 = 10%
    MetricsEngine engine(db, 1.0);
    MetricResult res = engine.calculateDataBusUtilization(8, 2); // BL8, DDR

    ASSERT_NEAR(res.value, 10.0, 0.01);
}

// -------------------------------------------------------------------------
// LPDDR4 JEDEC Test Cases (Special characteristics: 16-bit channel, different timings)
// -------------------------------------------------------------------------

TEST_F(MetricsEngineTest, LPDDR4_WriteToReadTurnaround)
{
    // Simulate LPDDR4 tWTR (Write to Read Turnaround)
    // LPDDR4 requires a larger turnaround delay compared to DDR4 due to internal architecture.
    
    // Insert a WRITE at 1000 ps
    insertPhase("WRITE", 1000, 1500);
    // Insert a READ at 8500 ps (Delay = 7500 ps = 7.5 ns)
    insertPhase("READ", 8500, 9000);

    // Insert another WRITE at 20000 ps
    insertPhase("WRITE", 20000, 20500);
    // Insert a READ at 28500 ps (Delay = 8500 ps = 8.5 ns)
    insertPhase("READ", 28500, 29000);

    MetricsEngine engine(db, 1.0);
    MetricResult res = engine.calculateWriteToReadTurnaround();

    // Average turnaround = (7.5 + 8.5) / 2 = 8.0 ns
    ASSERT_NEAR(res.value, 8.0, 0.01);
}

TEST_F(MetricsEngineTest, LPDDR4_RefreshOverhead)
{
    // LPDDR4 has per-bank refresh and all-bank refresh. Let's test overall REF time.
    // Total sim time = 2,000,000 ps (2000 ns)
    setSimulationTime(2000000.0);

    // Insert 4 REF commands
    insertPhase("REF", 10000, 11000);
    insertPhase("REFPB", 30000, 31000);
    insertPhase("REF", 50000, 51000);
    insertPhase("REF", 70000, 71000);

    // Assume LPDDR4 tRFC is 130 ns (130,000 ps).
    // 4 REFs * 130ns = 520 ns.
    // Overhead = 520 / 2000 = 26%
    double tRFC_ns = 130.0;

    MetricsEngine engine(db, 1.0);
    MetricResult res = engine.calculateRefreshOverhead(tRFC_ns);

    ASSERT_NEAR(res.value, 26.0, 0.01);
}

// -------------------------------------------------------------------------
// Missing Coverage Test Cases (100% Coverage Goal)
// -------------------------------------------------------------------------

TEST(MetricsEngineExceptions, ClosedDatabase)
{
    QSqlDatabase closedDb = QSqlDatabase::addDatabase("QSQLITE", "closed_db");
    // Do not open it
    EXPECT_THROW({
        MetricsEngine engine(closedDb, 1.0);
    }, std::runtime_error);
    QSqlDatabase::removeDatabase("closed_db");
}

TEST_F(MetricsEngineTest, DDR4_CommandBusUtilization)
{
    // Total simulation time = 10,000,000 ps (10,000 ns)
    setSimulationTime(10000000.0);
    
    // Simulate 500 commands (phases)
    for (int i = 0; i < 500; i++)
        insertPhase("ACT", i*10, i*10+5); // Phase name doesn't matter for this metric

    // Clock period = 2.0 ns (500 MHz)
    MetricsEngine engine(db, 2.0);
    MetricResult res = engine.calculateCommandBusUtilization();

    // Total cycles = 10,000 ns / 2.0 ns = 5000 cycles
    // Utilization = 500 commands / 5000 cycles = 10%
    ASSERT_NEAR(res.value, 10.0, 0.01);
}

TEST_F(MetricsEngineTest, DDR4_PageEfficiency)
{
    // Currently, calculatePageEfficiency is a stub (WIP) returning 0.0.
    // We just test to ensure it executes without crashing and returns the placeholder.
    MetricsEngine engine(db, 1.0);
    MetricResult res = engine.calculatePageEfficiency();
    
    ASSERT_NEAR(res.value, 0.0, 0.01);
}


