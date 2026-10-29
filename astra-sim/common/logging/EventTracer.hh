// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-sim/common/SimObject.hh>
#include <fstream>

namespace AstraSim {

class EventTracer {
  public:
    EventTracer();
    ~EventTracer();
    void init(const std::string& trace_filename);
    void enable();
    void disable();

    template <typename Func, typename... Args>
    void trace(const SimObject* obj_p, Func&& func, Args&&... args) {
        if (!m_enabled) {
            return;
        }

        // Logging timestamp (in ns) and object name
        if (obj_p != nullptr) {
            m_file << static_cast<uint64_t>(obj_p->curTick() / 1000)  // ns
                   << "," << obj_p->name() << ",";
        }

        // Call the provided function to log additional details
        func(m_file, std::forward<Args>(args)...);
        m_file << std::endl;
    }

  private:
    bool m_enabled;        // is tracing currently enabled?
    std::ofstream m_file;  // output file
};

/** Global variables for different tracers */

// Wavefront activity tracer
extern EventTracer g_wavefront_tracer;

// Functional/Verification tracer
extern EventTracer g_functional_tracer;

}  // namespace AstraSim
