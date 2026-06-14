#include "camclaw/TraceService.h"

namespace camclaw {

void TraceService::record(
    const std::string& trace_id,
    const std::string& stage,
    const std::string& name,
    const std::string& object_id,
    const std::string& message)
{
    events_.push_back(TraceEvent(trace_id, stage, name, object_id, message));
}

std::vector<TraceEvent> TraceService::eventsForTrace(const std::string& trace_id) const
{
    std::vector<TraceEvent> result;
    for (std::size_t index = 0u; index < events_.size(); ++index) {
        if (events_[index].trace_id == trace_id) {
            result.push_back(events_[index]);
        }
    }
    return result;
}

bool TraceService::contains(const std::string& trace_id, const std::string& name) const
{
    for (std::size_t index = 0u; index < events_.size(); ++index) {
        if (events_[index].trace_id == trace_id && events_[index].name == name) {
            return true;
        }
    }
    return false;
}

} // namespace camclaw
