// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>

#include <astra-sim/common/logging/EventTracer.hh>
#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/system/gpu/Wavefront.hh>

// Declare global tracer
namespace AstraSim {
EventTracer g_wavefront_tracer;   // tracing wavefront-level events
EventTracer g_functional_tracer;  // tracing functional/verification operations
}  // namespace AstraSim

using namespace AstraSim;

EventTracer::EventTracer() : m_enabled(false) {}

EventTracer::~EventTracer() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

void EventTracer::init(const std::string& trace_filename) {
    assert(!m_file.is_open());
    m_file.open(trace_filename);

    if (!m_file.is_open()) {
        THROW_ERR_NR("Failed to open %s for GPU op tracing\n", trace_filename);
    }
}

void EventTracer::enable() {
    if (m_enabled) {
        THROW_ERR_NR("EventTracer is already enabled: enabled=%d\n", m_enabled);
    }
    if (!m_file.is_open()) {
        THROW_ERR_NR("There is no open output trace file: is_open=%d\n",
                     m_file.is_open());
    }
    m_enabled = true;
}

void EventTracer::disable() {
    if (!m_enabled) {
        THROW_ERR_NR("EventTracer is not currently enabled: enabled=%d\n",
                     m_enabled);
    }
    m_enabled = false;
}
