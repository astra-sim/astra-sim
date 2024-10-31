/**
 * Implementation of setup_error in spdlog_setup.
 * @author Chen Weiguang
 * @version 0.3.3-pre
 */

#pragma once

#include <exception>
#include <string>

namespace spdlog_setup {
// declaration section

/**
 * Set-up error with textual description.
 */
class setup_error : public std::exception {
  public:
    /**
     * Constructor accepting the error message.
     * @param msg Error message to contain.
     * @throw std::bad_alloc
     */
    setup_error(const char *const msg);

    /**
     * Constructor accepting the error message.
     * @param msg Error message to contain.
     * @throw std::bad_alloc
     */
    setup_error(std::string msg);

    /**
     * Returns the error message.
     * @return Error message.
     */
    auto what() const noexcept -> const char * override;

  private:
    std::string msg;
};

// implementation section

inline setup_error::setup_error(const char *const msg) : msg(msg) {}

inline setup_error::setup_error(std::string msg) : msg(std::move(msg)) {}

inline auto setup_error::what() const noexcept -> const char * {
    return msg.c_str();
}
} // namespace spdlog_setup
