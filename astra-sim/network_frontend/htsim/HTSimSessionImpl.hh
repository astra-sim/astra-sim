#pragma once

#include "HTSimSession.hh"
#include "eventlist.h"

namespace HTSim {

class HTSimSession::HTSimSessionImpl {
    public:
        EventList eventlist;

        // Pure abstract methods
        virtual void run(const HTSim::tm_info* const tm) = 0;
        virtual void finish() = 0;
        virtual void schedule_htsim_event(FlowInfo flow, int flow_id, std::optional<uint32_t> msgid) = 0;
        virtual ~HTSimSessionImpl() = default;

        void stop_simulation();
};

} // namespace HTSim
