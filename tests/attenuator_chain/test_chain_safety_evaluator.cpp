// Pure-logic tests for ChainSafetyEvaluator. Pins down the incident-power
// math (input minus cumulative drop), NaN-rated stages being skipped, the
// firstOverloadIdx accounting, and the order-sensitivity of the chain.

#include <QTest>
#include <cmath>
#include <limits>
#include "chainsafetyevaluator.h"

static constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
static constexpr double kEps = 1e-9;

class TestChainSafetyEvaluator : public QObject
{
    Q_OBJECT

private slots:
    void emptyChain_returnsInputAtMeter_noOverload();
    void singleRatedStage_atRating_isOk_zeroHeadroom();
    void singleRatedStage_overRating_flagsOverload_negativeHeadroom();
    void nanRatedStage_isAlwaysUnknown_neverTrips();
    void cascade_onlyDownstreamStageOverloaded();
    void cascade_orderSwap_changesOutcome();
};

void TestChainSafetyEvaluator::emptyChain_returnsInputAtMeter_noOverload()
{
    const auto report = ChainSafetyEvaluator::evaluate(12.5, {});
    QVERIFY(report.perStage.isEmpty());
    QCOMPARE(report.firstOverloadIdx, -1);
    QCOMPARE(report.finalAtMeterDbm, 12.5);
}

void TestChainSafetyEvaluator::singleRatedStage_atRating_isOk_zeroHeadroom()
{
    QList<StageInfo> stages {
        { QStringLiteral("Fixed 10dB"), 10.0, 20.0, false }
    };
    const auto report = ChainSafetyEvaluator::evaluate(20.0, stages);
    QCOMPARE(report.perStage.size(), 1);
    const StageReport &r = report.perStage.first();
    QCOMPARE(r.incidentDbm, 20.0);
    QCOMPARE(r.ratedDbm, 20.0);
    QCOMPARE(r.headroomDb, 0.0);
    QVERIFY(r.status == StageStatus::Ok);
    QCOMPARE(report.firstOverloadIdx, -1);
    QCOMPARE(report.finalAtMeterDbm, 10.0); // 20 - 10
}

void TestChainSafetyEvaluator::singleRatedStage_overRating_flagsOverload_negativeHeadroom()
{
    QList<StageInfo> stages {
        { QStringLiteral("Digital +20dBm"), 6.0, 20.0, false }
    };
    const auto report = ChainSafetyEvaluator::evaluate(25.0, stages);
    QCOMPARE(report.perStage.size(), 1);
    const StageReport &r = report.perStage.first();
    QCOMPARE(r.incidentDbm, 25.0);
    QCOMPARE(r.ratedDbm, 20.0);
    QCOMPARE(r.headroomDb, -5.0);
    QVERIFY(r.status == StageStatus::Overload);
    QCOMPARE(report.firstOverloadIdx, 0);
}

void TestChainSafetyEvaluator::nanRatedStage_isAlwaysUnknown_neverTrips()
{
    QList<StageInfo> stages {
        { QStringLiteral("Cable"), 1.5, kNaN, false }
    };
    const auto report = ChainSafetyEvaluator::evaluate(100.0, stages);
    QCOMPARE(report.perStage.size(), 1);
    const StageReport &r = report.perStage.first();
    QCOMPARE(r.incidentDbm, 100.0);
    QVERIFY(std::isnan(r.ratedDbm));
    QVERIFY(std::isnan(r.headroomDb));
    QVERIFY(r.status == StageStatus::Unknown);
    QCOMPARE(report.firstOverloadIdx, -1);
    QCOMPARE(report.finalAtMeterDbm, 98.5);
}

void TestChainSafetyEvaluator::cascade_onlyDownstreamStageOverloaded()
{
    // Upstream stage soaks up 30 dB so stage 0 sees +25 dBm (within +30
    // rating) but stage 1 sees -5 dBm (well under). Flip the scenario so
    // only the downstream stage is over: stage 0 attenuates only 1 dB,
    // stage 1 sees almost the full input and trips its low rating.
    QList<StageInfo> stages {
        { QStringLiteral("Cable"), 1.0, kNaN, false },
        { QStringLiteral("Digital"), 6.0, 20.0, false }
    };
    const auto report = ChainSafetyEvaluator::evaluate(25.0, stages);
    QCOMPARE(report.perStage.size(), 2);
    QVERIFY(report.perStage.at(0).status == StageStatus::Unknown);
    QVERIFY(report.perStage.at(1).status == StageStatus::Overload);
    QCOMPARE(report.perStage.at(1).incidentDbm, 24.0); // 25 - 1
    QCOMPARE(report.firstOverloadIdx, 1);
}

void TestChainSafetyEvaluator::cascade_orderSwap_changesOutcome()
{
    // 30 dB fixed (rated +30) followed by 6 dB digital (rated +20). The
    // physical order is rated 30 first, so stage 0 (30 dB) sees +25 and is
    // fine, stage 1 (6 dB) sees -5 and is fine. Swap them and stage 0
    // becomes the 6 dB digital seeing +25 which exceeds its +20 rating.
    StageInfo fixed30 { QStringLiteral("Fixed 30dB"), 30.0, 30.0, false };
    StageInfo digital { QStringLiteral("Digital 6dB"), 6.0, 20.0, false };

    {
        QList<StageInfo> stages { fixed30, digital };
        const auto report = ChainSafetyEvaluator::evaluate(25.0, stages);
        QCOMPARE(report.firstOverloadIdx, -1);
    }
    {
        QList<StageInfo> stages { digital, fixed30 };
        const auto report = ChainSafetyEvaluator::evaluate(25.0, stages);
        QCOMPARE(report.firstOverloadIdx, 0);
        QCOMPARE(report.perStage.at(0).headroomDb, -5.0);
    }
}

QTEST_APPLESS_MAIN(TestChainSafetyEvaluator)
#include "test_chain_safety_evaluator.moc"
