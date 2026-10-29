// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <astra-network-analytical/common/Event.h>
#include <astra-network-analytical/common/Type.h>
#include <astra-sim/common/AstraNetworkAPI.hh>
#include <astra-sim/common/Common.hh>
#include <common/Types.hh>
#include <memory>
#include <optional>

using namespace AstraSim;
using namespace AstraSimAnalytical;
using namespace NetworkAnalytical;

namespace AstraSimAnalytical {

/**
 * ChunkTrackerEntry manages state of any transaction. Base/virtual class for
 * ease of extension
 *
 */
class ChunkTrackerEntry {

  public:
    /**
     * Constructor and destructor
     */
    ChunkTrackerEntry() noexcept;
    virtual ~ChunkTrackerEntry() {};

    /**
     * Get the send request
     *
     * @return pointer to the send request
     */
    [[nodiscard]] const analytical_network_request* get_send_request()
        const noexcept;

    /**
     * Set the send_request field. Copies relevant information from the
     * sim_request into an
     *
     * @param request request provided by sim_send
     */
    void set_send_request_from_sim_request(const sim_request* request,
                                           const Sys_ID src_id) noexcept;

    /**
     * Query if the chunk is done transmitting
     *
     * @return true if current_request_state == Completed
     */
    [[nodiscard]] virtual bool is_completed() const noexcept;

    /**
     * Getting current_request state from FSM
     *
     * @return state of type enum AllChunkStates
     */
    [[nodiscard]] virtual AllChunkStates get_current_state() const noexcept;

    /**
     * Getting original sending system. Mainly for getting system pointer for
     * callbacks from static functions
     *
     * @return ID of system that initiated sim_cmd
     */
    [[nodiscard]] virtual Sys_ID get_original_system_id() const noexcept;

    /**
     * Set last sent tick. Usually for stats
     *
     * @param Tick of current transmission beginning
     */
    virtual void set_transmission_tick(Tick val) noexcept;

    /**
     * Get last sent tick. Usually for stats
     *
     * @return Tick of last transmission beginning
     */
    virtual Tick get_transmission_tick() const noexcept;

    /**
     * Type of this chunk entry. hardcoded response
     *
     * @return AstraNetworkAPI::CommandType type
     */
    virtual AstraNetworkAPI::CommandType get_chunk_type() noexcept {
        return AstraNetworkAPI::CommandType::Invalid;
    };

    /**
     * Transition to next state. Major function to be defined by derived classes
     *
     */
    virtual void transition_next_state() noexcept = 0;

  protected:
    // current "state" of a chunk
    AllChunkStates current_state;

    // original sim_request for the sim_send
    analytical_network_request send_request;

    // last tick of transmission. mainly for stats
    Tick send_tick;
};

class LocalLoadChunkTrackerEntry : public ChunkTrackerEntry {

  public:
    LocalLoadChunkTrackerEntry();

    /**
     * Transition to next state. Major function to be defined by derived classes
     *
     */
    virtual void transition_next_state() noexcept override;

    /**
     * Type of this chunk entry. hardcoded response
     *
     * @return AstraNetworkAPI::CommandType type
     */
    virtual AstraNetworkAPI::CommandType get_chunk_type() noexcept override {
        return AstraNetworkAPI::CommandType::Local_Load;
    };
};

class LocalStoreChunkTrackerEntry : public ChunkTrackerEntry {

  public:
    explicit LocalStoreChunkTrackerEntry(uint64_t vaddr);

    /**
     * Transition to next state. Major function to be defined by derived classes
     *
     */
    virtual void transition_next_state() noexcept override;

    /**
     * Type of this chunk entry. hardcoded response
     *
     * @return AstraNetworkAPI::CommandType type
     */
    virtual AstraNetworkAPI::CommandType get_chunk_type() noexcept override {
        return AstraNetworkAPI::CommandType::Local_Store;
    };

    uint64_t vaddr() const noexcept;

  private:
    /**
     * @brief Address to store to
     */
    uint64_t _vaddr;
};

class RemoteLoadChunkTrackerEntry : public ChunkTrackerEntry {

  public:
    RemoteLoadChunkTrackerEntry();

    /**
     * Transition to next state. Major function to be defined by derived classes
     *
     */
    virtual void transition_next_state() noexcept override;

    /**
     * Type of this chunk entry. hardcoded response
     *
     * @return AstraNetworkAPI::CommandType type
     */
    virtual AstraNetworkAPI::CommandType get_chunk_type() noexcept override {
        return AstraNetworkAPI::CommandType::Remote_Load;
    };
};

class RemoteStoreChunkTrackerEntry : public ChunkTrackerEntry {

  public:
    explicit RemoteStoreChunkTrackerEntry(uint64_t vaddr);

    /**
     * Transition to next state. Major function to be defined by derived classes
     *
     */
    virtual void transition_next_state() noexcept override;

    /**
     * Type of this chunk entry. hardcoded response
     *
     * @return AstraNetworkAPI::CommandType type
     */
    virtual AstraNetworkAPI::CommandType get_chunk_type() noexcept override {
        return AstraNetworkAPI::CommandType::Remote_Store;
    };

    uint64_t vaddr() const noexcept;

  private:
    /**
     * @brief Address to store to
     */
    uint64_t _vaddr;
};

/**
 * Models a plain rank-to-rank Send/Recv transfer (the low-level primitive the
 * collective algorithms use): no buffer/CU addressing, a single hop straight
 * from the source rank's network endpoint to the destination's.
 *
 * A Send and its matching Recv share the same req_id (see ReqId.hh), so
 * whichever of the two calls sim_cmd() first constructs this entry and the
 * second one finds it. It starts at Unstarted rather than a running state:
 * only the Send side has the transfer parameters, so it alone calls
 * set_send_request_from_sim_request() then transition_next_state() to move
 * Unstarted -> SrcToDstGPU_Data and kick off the FSM.
 */
class SendRecvChunkTrackerEntry : public ChunkTrackerEntry {

  public:
    SendRecvChunkTrackerEntry() noexcept;

    /**
     * Transition to next state. Major function to be defined by derived classes
     *
     */
    virtual void transition_next_state() noexcept override;

    /**
     * Type of this chunk entry. hardcoded response
     *
     * @return AstraNetworkAPI::CommandType type
     */
    virtual AstraNetworkAPI::CommandType get_chunk_type() noexcept override {
        return AstraNetworkAPI::CommandType::Send;
    };

    /**
     * Mark that the Recv side has posted and is waiting on completion.
     */
    void mark_recv_posted() noexcept;

    /**
     * @return true if the Recv side has posted and is waiting on completion
     */
    bool is_recv_posted() const noexcept;

  private:
    bool _recv_posted;
};

}  // namespace AstraSimAnalytical
