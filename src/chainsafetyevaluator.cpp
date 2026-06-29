#include "chainsafetyevaluator.h"
#include <cmath>
#include <limits>

ChainReport ChainSafetyEvaluator::evaluate(double inputDbm, const QList<StageInfo> &stages)
{
    ChainReport report;
    report.firstOverloadIdx = -1;
    double cumulativeDropDb = 0.0;

    for (int i = 0; i < stages.size(); ++i)
        {
            const StageInfo &s = stages.at(i);
            StageReport r;
            r.stageIdx = i;
            r.incidentDbm = inputDbm - cumulativeDropDb;
            r.ratedDbm = s.maxInputDbm;
            // Unknown when the rating is unset OR the incident power is
            // poisoned by an upstream NaN effectiveDb. The second branch
            // matters because `NaN > finite` is false, which would
            // otherwise silently mark every downstream stage Ok and erase
            // real overload warnings.
            if (std::isnan(s.maxInputDbm) || std::isnan(r.incidentDbm))
                {
                    r.headroomDb = std::numeric_limits<double>::quiet_NaN();
                    r.status = StageStatus::Unknown;
                }
            else
                {
                    r.headroomDb = s.maxInputDbm - r.incidentDbm;
                    if (r.incidentDbm > s.maxInputDbm)
                        {
                            r.status = StageStatus::Overload;
                            if (report.firstOverloadIdx < 0)
                                report.firstOverloadIdx = i;
                        }
                    else
                        {
                            r.status = StageStatus::Ok;
                        }
                }
            report.perStage.append(r);
            cumulativeDropDb += s.effectiveDb;
        }

    report.finalAtMeterDbm = inputDbm - cumulativeDropDb;
    return report;
}
