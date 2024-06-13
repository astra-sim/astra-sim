/**
 * Implementation of the mini template engine in spdlog_setup.
 * @author Chen Weiguang
 * @version 0.3.3-pre
 */

#pragma once

#include "setup_error.h"

#include "spdlog/fmt/fmt.h"

#include <sstream>
#include <string>
#include <unordered_map>

namespace spdlog_setup {
namespace details {
// declaration section

enum class render_state {
    text,
    var_starting,
    var,
    var_name_start,
    var_name_done,
    var_ending,
    verbatim_double,
};

inline auto is_valid_var_char(const char c) -> bool {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

inline auto render(
    const std::string &tmpl,
    const std::unordered_map<std::string, std::string> &m) -> std::string {
    // fmt
    using fmt::format;

    // std
    using std::stringstream;

    stringstream output;
    stringstream var_buffer;
    auto state = render_state::text;

    for (const auto c : tmpl) {
        switch (state) {
        case render_state::text:
            switch (c) {
            case '{':
                state = render_state::var_starting;
                break;
            default:
                output << c;
                break;
            }
            break;

        case render_state::var_starting:
            switch (c) {
            case '{':
                state = render_state::var;
                break;
            default:
                output << '{' << c;
                break;
            }
            break;

        case render_state::var:
            switch (c) {
            case ' ':
                // chomp away
                break;
            case '"':
                state = render_state::verbatim_double;
                break;
            case '}':
                state = render_state::var_ending;
                break;
            default:
                if (!is_valid_var_char(c)) {
                    throw setup_error(format(
                        "Found invalid char '{}' in variable interpolation",
                        c));
                }
                state = render_state::var_name_start;
                var_buffer << c;
                break;
            }
            break;

        case render_state::var_name_start:
            switch (c) {
            case ' ':
                state = render_state::var_name_done;
                break;
            case '}':
                state = render_state::var_ending;
                break;
            default:
                if (!is_valid_var_char(c)) {
                    throw setup_error(
                        format("Found invalid char '{}' in variable name", c));
                }
                var_buffer << c;
                break;
            }
            break;

        case render_state::var_name_done:
            switch (c) {
            case ' ':
                // chomp away
                break;
            case '}':
                state = render_state::var_ending;
                break;
            default:
                throw setup_error(format(
                    "Found invalid char '{}' after variable name '{}'",
                    c,
                    var_buffer.str()));
            }
            break;

        case render_state::var_ending:
            switch (c) {
            case '}': {
                state = render_state::text;
                const auto var_value_itr = m.find(var_buffer.str());
                var_buffer.str("");

                if (var_value_itr != m.cend()) {
                    output << var_value_itr->second;
                }
                break;
            }
            default:
                throw setup_error(
                    format("Found invalid char '{}' when expecting '}}'", c));
            }
            break;

        case render_state::verbatim_double:
            switch (c) {
            case '"':
                state = render_state::var_name_done;
                break;
            default:
                output << c;
                break;
            }
            break;

        default:
            throw setup_error(
                "Reached impossible case while rendering template");
        }
    }

    if (state != render_state::text) {
        throw setup_error("Invalid state reached after parsing last char");
    }

    return output.str();
}
} // namespace details
} // namespace spdlog_setup
