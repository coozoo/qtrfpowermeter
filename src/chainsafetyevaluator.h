#ifndef CHAINSAFETYEVALUATOR_H
#define CHAINSAFETYEVALUATOR_H

#include <QList>
#include <QString>
#include <QMetaType>

// One stage in the attenuator chain, top (= source side) to bottom
// (= meter side). effectiveDb already folds in insertion loss for digital
// attenuators (per phase 6c), so the evaluator stays frequency-unaware.
struct StageInfo
{
    QString name;
    double effectiveDb;
    double maxInputDbm;   // NaN means "rating unknown, do not check"
    bool isInternal;      // pinned device-front-end stage
};

enum class StageStatus
{
    Ok,
    Overload,
    Unknown   // ratedDbm is NaN, no check is possible
};

struct StageReport
{
    int stageIdx;
    double incidentDbm;
    double ratedDbm;
    double headroomDb;   // ratedDbm - incidentDbm; negative when overloaded
    StageStatus status;
};

// Walks a chain end-to-end; returns the incident power on each stage plus a
// summary. Pure: no Qt widgets, no globals, no I/O. Tested under
// tests/attenuator_chain/.
struct ChainReport
{
    QList<StageReport> perStage;
    double finalAtMeterDbm;
    int firstOverloadIdx;   // -1 if no stage is in Overload
};

class ChainSafetyEvaluator
{
public:
    static ChainReport evaluate(double inputDbm, const QList<StageInfo> &stages);
};

Q_DECLARE_METATYPE(StageInfo)
Q_DECLARE_METATYPE(ChainReport)

#endif // CHAINSAFETYEVALUATOR_H
