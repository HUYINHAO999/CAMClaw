#ifndef CAMCLAW_TRACE_SERVICE_H
#define CAMCLAW_TRACE_SERVICE_H

#include <string>
#include <vector>

namespace camclaw {

struct TraceEvent {
    TraceEvent()
    {
    }

    TraceEvent(
        const std::string& trace,
        const std::string& event_stage,
        const std::string& event_name,
        const std::string& related_object,
        const std::string& event_message)
        : trace_id(trace),
          stage(event_stage),
          name(event_name),
          object_id(related_object),
          message(event_message)
    {
    }

    std::string trace_id;
    std::string stage;
    std::string name;
    std::string object_id;
    std::string message;
};

class TraceService {
public:
    void record(
        const std::string& trace_id,
        const std::string& stage,
        const std::string& name,
        const std::string& object_id,
        const std::string& message);

    std::vector<TraceEvent> eventsForTrace(const std::string& trace_id) const;
    bool contains(const std::string& trace_id, const std::string& name) const;

private:
    std::vector<TraceEvent> events_;
};

} // namespace camclaw

#endif // CAMCLAW_TRACE_SERVICE_H
