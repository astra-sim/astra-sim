// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <fstream>

#include <astra-sim/common/logging/Logger.hh>
#include <astra-sim/system/gpu/msccl/MscclOps.hh>
#include <astra-sim/system/gpu/msccl/Parser.hh>

using namespace AstraSim;

//------------------------------------------------------------------------------
// GpuParser
//------------------------------------------------------------------------------
// Parser for GPU-level MSCCL++ plan from JSON file

Msccl::GpuParser::GpuParser(const std::string& json_file,
                            uint32_t _gpu_id,
                            uint32_t _unrolling_factor,
                            uint64_t _chunk_nbytes,
                            uint32_t _wfreq_nbytes)
    : gpu_id(_gpu_id),
      unrolling_factor(_unrolling_factor),
      chunk_nbytes(_chunk_nbytes),
      wfreq_nbytes(_wfreq_nbytes) {

    // Sanity checks
    // assert(unrolling_factor > 0);
    assert(wfreq_nbytes > 0);

    // Open and read the file into JSON object
    std::ifstream input_file(json_file);
    if (!input_file.is_open()) {
        THROW_ERR_NR("[Msccl::Parser] Custom collective JSON file"
                     " (at: %s) loading unsuccessful.\n",
                     json_file.c_str());
    }

    json j;
    try {
        input_file >> j;
    } catch (const json::parse_error& e) {
        THROW_ERR_NR("[Msccl::Parser] JSON parsing error in file %s: %s\n",
                     json_file.c_str(), e.what());
    }

    /** Parse the json file */
    try {
        // Parse the protocol information
        protocol = getProtocolTypeFromString(j["protocol"].get<std::string>());
        assert(unrolling_factor > 0);
        DPRINTFNR("Protocol found: %s; Unrolling Factor set to = %d\n",
                  protocolTypeToString(protocol), unrolling_factor);

        // Extract GPU info
        assert(j["gpus"].is_array());
        assert(gpu_id < j["gpus"].size());
        const auto& j_gpu = j["gpus"][gpu_id];
        assert(j_gpu["id"].get<uint32_t>() == gpu_id);

        unit_chunk_cnt = j.value("unit_chunk_cnt", 0);
        DPRINTFNR("Unit Chunk Count = %d\n", unit_chunk_cnt);

        // Parse number of chunks
        num_buf_chunks[Msccl::BufferType::Input] =
            j_gpu["input_chunks"].get<uint32_t>();
        num_buf_chunks[Msccl::BufferType::Output] =
            j_gpu["output_chunks"].get<uint32_t>();
        num_buf_chunks[Msccl::BufferType::Scratch] =
            j_gpu["scratch_chunks"].get<uint32_t>();

        // chunk_nbytes may legitimately be 0 for plans with no data buffers
        // (pure synchronization plans); it is required only when buffers exist.
        const auto total_buf_chunks =
            num_buf_chunks[Msccl::BufferType::Input] +
            num_buf_chunks[Msccl::BufferType::Output] +
            num_buf_chunks[Msccl::BufferType::Scratch];
        if ((total_buf_chunks > 0) && (chunk_nbytes == 0)) {
            THROW_ERR_NR("[Msccl::Parser] Plan %s has %u buffer chunks but "
                         "chunk_nbytes == 0\n",
                         json_file.c_str(), total_buf_chunks);
        }

        // Parse number of threads per threadblock
        num_threads_per_tb = j["num_threads_per_block"].get<uint32_t>();
        assert(num_threads_per_tb > 0);

        // Parse channels
        parseChannels(j_gpu["channels"]);

        // Parse remote buffers
        parseRemoteBuffers(j_gpu["remote_buffers"]);

        // Parse semaphores
        parseLocalSemaphores(j_gpu["semaphores"]);

        // Determine if inplace or not.
        inplace = j["inplace"].get<bool>();

        // Construct threadblock parsers for parsing TB-level operations
        for (const auto& j_tb : j_gpu["threadblocks"]) {
            tb_parsers.emplace_back(*this, j_tb["id"].get<uint32_t>(),
                                    unit_chunk_cnt, j_tb);
        }

        // Obtain the expected memory contents. Memory does not need to be
        // included; if it's not, then the verifier will not check it.
        if (j["memory"].is_array()) {
            for (const auto& item : j["memory"]) {
                if (item["rank_index"].get<uint32_t>() == gpu_id) {
                    parseMemory(item["output_buffer"]);
                    break;
                }
            }
            // It's not a requirement that every GPU has an expected memory
            // format. If memory is not included for this GPU (if parseMemory
            // did not run for us), then our expected memory is empty, and the
            // verifier will always pass for us.
        }
    } catch (const std::exception& e) {
        THROW_ERR_NR("[Msccl::Parser] Error while parsing JSON file %s: %s\n",
                     json_file.c_str(), e.what());
    }
}

void Msccl::GpuParser::parseChannels(const nlohmann::json& j_chan_info) {
    // Parse this JSON structures into mem_chan_lst and swt_chan_lst
    //  "channels": [
    //    {
    //      "channel_type": "memory",
    //      "connected_to": [
    //        0,
    //        0
    //      ]
    //    },
    //    {
    //      "channel_type": "switch",
    //      "buffer_type": "s",
    //      "rank_groups": [
    //        {
    //          "size": 2,
    //          "ranks": [
    //            0,
    //            1
    //          ]
    //        }
    //      ]
    //    }
    //  ],
    for (const auto& j_chan : j_chan_info) {
        if (j_chan["channel_type"] == "memory") {
            assert(mem_chan_lst.empty());
            std::unordered_map<uint32_t, uint32_t> rank_cnt_map;
            for (const auto& remote_rank :
                 j_chan["connected_to"].get<std::vector<uint32_t>>()) {
                if (rank_cnt_map.count(remote_rank) == 0) {
                    rank_cnt_map[remote_rank] = 0;
                }
                // Channel ID is defined as the relative index of this channel
                // among all channels connected to the same rank
                auto mem_chan_id = rank_cnt_map.at(remote_rank);
                mem_chan_lst.emplace_back(remote_rank, mem_chan_id);
                rank_cnt_map[remote_rank]++;
            }
        } else if (j_chan["channel_type"] == "switch") {
            assert(swt_chan_lst.empty());
            auto buf_type = getBufferTypeFromString(
                j_chan["buffer_type"].get<std::string>());
            for (const auto& rank_group : j_chan["rank_groups"]) {
                swt_chan_lst.emplace_back(
                    rank_group["ranks"].get<std::vector<int>>(), buf_type);
            }
        } else {
            // TODO: support port channels
            THROW_ERR_NR("[Msccl::Parser] Unsupported channel type: %s\n",
                         j_chan["channel_type"].get<std::string>().c_str());
        }
    }
}

void Msccl::GpuParser::parseRemoteBuffers(const nlohmann::json& j_buf_info) {
    // Parse this JSON structures into rem_buf_lst
    // "remote_buffers": [
    //   {
    //     "rank": 0,
    //     "type": "i",
    //     "access_channel_types": [
    //       "memory"
    //     ]
    //   }
    // ]
    for (const auto& j_buf : j_buf_info) {
        auto buf_type =
            getBufferTypeFromString(j_buf["type"].get<std::string>());
        auto buf_nbytes = chunk_nbytes * num_buf_chunks.at(buf_type);
        std::vector<ChannelType> access_chan_types;
        for (const auto& chan_type_str :
             j_buf["access_channel_types"].get<std::vector<std::string>>()) {
            access_chan_types.push_back(
                getChannelTypeFromString(chan_type_str));
        }
        const auto planChunk =
            planChunkNbytes(chunk_nbytes, protocol, buf_type, wfreq_nbytes);
        rem_buf_lst.emplace_back(
            j_buf["rank"].get<uint32_t>(),  // rank where this buffer is located
            buf_type,                       // buffer type
            0,                              // byte offset
            buf_nbytes,                     // buffer size in bytes
            access_chan_types, planChunk.data_in_wfreq,
            planChunk.flag_in_wfreq);
    }
}

void Msccl::GpuParser::parseLocalSemaphores(const nlohmann::json& j_sema_info) {
    // Parse this JSON structure into sema_lst
    //  "semaphores": [
    //    {
    //      "init_value": 0
    //    },
    //    {
    //      "init_value": 0
    //    }
    //  ]
    int sema_id = 0;
    for (const auto& j_sema : j_sema_info) {
        sema_lst.emplace_back(gpu_id, sema_id, j_sema["init_value"].get<int>(),
                              false);  // local semaphore for intra-GPU sync
        sema_id++;
    }
}

uint32_t Msccl::GpuParser::getNumThreadblocks() const {
    return tb_parsers.size();
}

GpuOpPtr Msccl::GpuParser::getFirstGpuOp(uint32_t tb_id) const {
    assert(tb_id < tb_parsers.size());
    const auto& tb_parser = tb_parsers[tb_id];
    assert(!tb_parser.op_lst.empty());
    return tb_parser.op_lst[0];
}

void Msccl::GpuParser::parseMemory(const nlohmann::json& j_buffer) {
    // Parse this JSON structure into expected_memory
    //  [
    //   {
    //    "chunk_index": 0,
    //    "chunk": [
    //     {
    //      "rank": 0
    //      "index": 0
    //     }, ...
    //    ]
    //   }, ...
    //  ]
    for (const auto& j_chunk_obj : j_buffer) {
        uint32_t chunk_index = j_chunk_obj["chunk_index"].get<uint32_t>();
        std::unordered_set<const TraceMemoryCell*> components;
        for (const auto& j_contents : j_chunk_obj["chunk"]) {
            uint32_t rank = j_contents["rank"];
            uint32_t index = j_contents["index"];
            components.insert(
                new TraceMemoryCellInit(rank, index, BufferType::Input));
        }

        expected_memory[chunk_index] =
            (components.size() > 1)
                ? new TraceMemoryCellReduction(components)
                : *components.begin();  // Exactly one element
    }
}

//------------------------------------------------------------------------------
// ThreadblockParser
//------------------------------------------------------------------------------
// Parser for threadblock-level MSCCL++ operations from JSON file

Msccl::ThreadblockParser::ThreadblockParser(const GpuParser& _gpu_parser,
                                            uint32_t _tb_id,
                                            uint32_t unit_chunk_cnt,
                                            const nlohmann::json& j_tb)
    : gpu_parser(_gpu_parser),
      tb_id(_tb_id) {
    // Construct TB-level channel lists
    for (const auto& chan_info : j_tb["channels"]) {
        if (chan_info["channel_type"] == "memory") {
            assert(tb_mem_chan_lst.empty());
            for (const auto& chan_idx :
                 chan_info["channel_ids"].get<std::vector<uint32_t>>()) {
                assert(chan_idx < gpu_parser.mem_chan_lst.size());
                tb_mem_chan_lst.push_back(gpu_parser.mem_chan_lst[chan_idx]);
            }
        } else if (chan_info["channel_type"] == "switch") {
            assert(tb_swt_chan_lst.empty());
            for (const auto& chan_idx :
                 chan_info["channel_ids"].get<std::vector<uint32_t>>()) {
                assert(chan_idx < gpu_parser.swt_chan_lst.size());
                tb_swt_chan_lst.push_back(gpu_parser.swt_chan_lst[chan_idx]);
            }
        } else {
            THROW_ERR_NR("[Msccl::Parser] Unsupported channel type in"
                         " threadblock: %s\n",
                         chan_info["channel_type"].get<std::string>().c_str());
        }
    }

    // Construct TB-level remote buffer list
    for (const auto& buf_ref : j_tb["remote_buffer_refs"]) {
        auto access_chan_type = getChannelTypeFromString(
            buf_ref["access_channel_type"].get<std::string>());
        for (auto idx :
             buf_ref["remote_buffer_ids"].get<std::vector<uint32_t>>()) {
            assert(idx < gpu_parser.rem_buf_lst.size());
            const auto& buf = gpu_parser.rem_buf_lst[idx];
            // Only add the buffer if it can be accessed by the specified
            // channel type
            if (std::find(buf.access_chan_types.begin(),
                          buf.access_chan_types.end(),
                          access_chan_type) != buf.access_chan_types.end()) {
                tb_rem_buf_lst.push_back(buf);
            } else {
                THROW_ERR_NR("[Msccl::Parser] BufferChunk (rank=%d, type=%d) "
                             "cannot be accessed by channel type %d\n",
                             buf.rank, static_cast<int>(buf.type),
                             static_cast<int>(access_chan_type));
            }
        }
    }

    // Parse operation list
    for (const auto& j_op : j_tb["ops"]) {
        if (j_op["name"] == "pipeline") {
            // Parse the iteration context info
            // "iter_context": {
            //    "unit_size": 1048576,
            //    "num_chunks": 1
            //  },

            // Each loop iteration processes unit_size bytes of data
            auto unit_size = j_op["iter_context"]["unit_size"].get<uint64_t>();
            assert(unit_size > 0);
            if (unit_size % gpu_parser.wfreq_nbytes != 0) {
                THROW_ERR_NR("[Msccl::Parser] Pipeline unit size (%d bytes) "
                             "is not a multiple of wavefront request size "
                             "(%d bytes)\n",
                             unit_size, gpu_parser.wfreq_nbytes);
            }

            // The entire pipeline loop processes num_chunks chunks of data
            auto num_chunks =
                j_op["iter_context"]["num_chunks"].get<uint32_t>();
            auto total_size = num_chunks * gpu_parser.chunk_nbytes;
            auto niters = (total_size % unit_size == 0)
                              ? (total_size / unit_size)
                              : (total_size / unit_size + 1);

            // Parse inner ops
            std::vector<GpuOpPtr> inner_op_lst;
            for (const auto& j_inner_op : j_op["ops"]) {
                auto op_p =
                    parseOp(j_inner_op, niters, unit_size, unit_chunk_cnt);
                inner_op_lst.push_back(op_p);
                op_lst.push_back(op_p);
            }

            // Set the backward jump from the last op to the first op
            assert(!inner_op_lst.empty());
            inner_op_lst.back()->setBranchTargetOpPtr(inner_op_lst[0]);
        } else {
            op_lst.push_back(parseOp(j_op, 1, 0, unit_chunk_cnt));
        }
    }

    // Chain ops together
    for (auto i = 0; i < (op_lst.size() - 1); ++i) {
        op_lst[i]->setNextOpPtr(op_lst[i + 1]);
    }

    // Debugging print
    for (auto& op_p : op_lst) {
        DPRINTFNR("[Msccl::ThreadblockParser] Parsed gpu=%d, wg=%d, op: %s, "
                  "next_op: %s\n",
                  gpu_parser.gpu_id, tb_id, op_p->to_string().c_str(),
                  op_p->getNextOpPtr()
                      ? op_p->getNextOpPtr()->to_string().c_str()
                      : "nullptr");
    }
}

Msccl::BufferChunk Msccl::ThreadblockParser::parseBufferChunk(
    const nlohmann::json& j_buf_chunk, uint32_t unit_chunk_cnt) {
    // Extract common fields
    auto chunk_index = j_buf_chunk["index"].get<uint32_t>();
    float chunk_factor = j_buf_chunk.value("chunk_factor", 1.0f);

    // Determine buffer properties based on whether it's remote or local
    bool is_remote = j_buf_chunk.contains("buffer_id");
    uint32_t rank, max_nchunks;
    Msccl::BufferType buf_type;
    std::vector<ChannelType> access_chan_types;

    if (is_remote) {
        auto buf_id = j_buf_chunk["buffer_id"].get<uint32_t>();
        assert(buf_id < tb_rem_buf_lst.size());
        const auto& rem_buf = tb_rem_buf_lst[buf_id].get();
        rank = rem_buf.rank;
        buf_type = rem_buf.type;
        max_nchunks = gpu_parser.num_buf_chunks.at(buf_type);
        access_chan_types = rem_buf.access_chan_types;
    } else {
        rank = gpu_parser.gpu_id;
        buf_type =
            getBufferTypeFromString(j_buf_chunk["type"].get<std::string>());
        max_nchunks = gpu_parser.num_buf_chunks.at(buf_type);
        access_chan_types = {ChannelType::Memory};
    }

    auto nchunks = j_buf_chunk.value("size", is_remote ? 0u : 1u);
    if (is_remote) {
        nchunks = j_buf_chunk["size"].get<uint32_t>();
    }

    const auto planChunk =
        planChunkNbytes(gpu_parser.chunk_nbytes, gpu_parser.protocol, buf_type,
                        gpu_parser.wfreq_nbytes);

    // Handle fixed-size workload
    if (unit_chunk_cnt == 0) {
        if ((chunk_index + nchunks) > max_nchunks) {
            THROW_ERR_NR("[Msccl::Parser] %s BufferChunk (type=%d) "
                         "chunk index + size out of bounds: index=%d, size=%d, "
                         "max_num_chunks=%d\n",
                         is_remote ? "Remote" : "Local",
                         static_cast<int>(buf_type), chunk_index, nchunks,
                         max_nchunks);
        }
        DPRINTFNR("[Msccl::Parser] %s buf: off=%lu original_nbytes=%lu "
                  "assigned_nbytes=%lu "
                  "unit_chunk_cnt=%d\n",
                  is_remote ? "Remote" : "Local",
                  static_cast<uint64_t>(chunk_index) * planChunk.nbytes,
                  static_cast<uint64_t>(gpu_parser.chunk_nbytes),
                  static_cast<uint64_t>(planChunk.nbytes),
                  static_cast<uint64_t>(
                      chunk_factor * static_cast<float>(unit_chunk_cnt) *
                      static_cast<float>(gpu_parser.chunk_nbytes)));
        return Msccl::BufferChunk(
            rank, buf_type,
            static_cast<uint64_t>(chunk_index) * planChunk.nbytes,
            static_cast<uint64_t>(nchunks) * planChunk.nbytes,
            access_chan_types, planChunk.data_in_wfreq,
            planChunk.flag_in_wfreq);
    }

    // Handle variable-size workload
    const uint64_t byte_offset = static_cast<uint64_t>(chunk_index) *
                                 gpu_parser.chunk_nbytes * unit_chunk_cnt;
    const uint64_t logical_nbytes = static_cast<uint64_t>(
        chunk_factor * static_cast<float>(unit_chunk_cnt) *
        static_cast<float>(gpu_parser.chunk_nbytes));
    const uint32_t wfreq = gpu_parser.wfreq_nbytes;
    const uint64_t nbytes = (logical_nbytes % wfreq == 0)
                                ? logical_nbytes
                                : ((logical_nbytes / wfreq) + 1) * wfreq;

    Msccl::BufferChunk buf_chunk(rank, buf_type, byte_offset, nbytes,
                                 access_chan_types, planChunk.data_in_wfreq,
                                 planChunk.flag_in_wfreq, chunk_factor);

    // Bounds check in bytes
    uint64_t max_bytes = static_cast<uint64_t>(max_nchunks) *
                         gpu_parser.chunk_nbytes * unit_chunk_cnt;
    if (buf_chunk.byte_offset + buf_chunk.nbytes > max_bytes) {
        THROW_ERR_NR("[Msccl::Parser] %s BufferChunk (type=%d) "
                     "byte_offset + nbytes out of bounds: byte_offset=%lu, "
                     "nbytes=%lu, max_bytes=%lu\n",
                     is_remote ? "Remote" : "Local", static_cast<int>(buf_type),
                     buf_chunk.byte_offset, buf_chunk.nbytes, max_bytes);
    }

    DPRINTFNR("[Msccl::Parser] %s buf: off=%lu nbytes=%lu factor=%f "
              "unit_chunk_cnt=%d aligned_to=%u\n",
              is_remote ? "Remote" : "Local", buf_chunk.byte_offset,
              buf_chunk.nbytes, chunk_factor, unit_chunk_cnt,
              gpu_parser.wfreq_nbytes);

    return buf_chunk;
}

GpuOpPtr Msccl::ThreadblockParser::parseOp(const nlohmann::json& j_op,
                                           uint64_t niters,
                                           uint64_t iter_nbytes,
                                           uint32_t unit_chunk_cnt) {
    auto op_name_str = j_op["name"].get<std::string>();

    if (op_name_str == "nop") {  // NOP
        /** Nop operation */
        auto op = std::make_shared<Msccl::NopOp>(niters);
        return op;
    } else if (op_name_str == "barrier") {  // BARRIER
        /** Barrier operation */
        auto op = std::make_shared<Msccl::BarrierOp>(
            j_op["barrier_id"].get<uint32_t>(),
            j_op["num_threadblocks"].get<uint32_t>(), niters);
        return op;
    } else if ((op_name_str == "signal") ||     // SIGNAL
               (op_name_str == "rlxsignal")) {  // Relaxed SIGNAL
        /** Signal (i.e., semaphore release) operation */
        // {
        //   "name": "signal",
        //   "channel_ids": [
        //     0,
        //     1,
        //     2
        //   ],
        //   "channel_type": "memory"
        // }
        // Parse channel type
        if (j_op["channel_type"] != "memory") {
            THROW_ERR_NR("[Msccl::Parser] Unsupported channel type in"
                         " threadblock signal op: %s\n",
                         j_op["channel_type"].get<std::string>().c_str());
        }

        // Parse channel IDs and create Semaphore objects
        std::vector<Semaphore> sema_lst;
        for (const auto& chan_id : j_op["channel_ids"]) {
            auto cid = chan_id.get<uint32_t>();
            if (j_op["channel_type"] == "memory") {
                assert(cid < tb_mem_chan_lst.size());
                const auto& mem_chan = tb_mem_chan_lst[cid].get();
                assert(mem_chan.rank != gpu_parser.gpu_id);
                // Figure out target remote GPU ID from memory channel
                sema_lst.emplace_back(
                    mem_chan.rank,      // ID of GPU where the semaphore resides
                    gpu_parser.gpu_id,  // Connected GPU is this GPU
                    mem_chan.chan_id,   // semaphore ID
                    0);                 // init value (always 0)
            }
        }
        assert(sema_lst.size() > 0);

        // Create SemReleaseOp
        auto op = std::make_shared<Msccl::SemReleaseOp>(
            op_name_str, sema_lst, BufferType::Semaphore,
            gpu_parser.wfreq_nbytes, niters);
        return op;
    } else if ((op_name_str == "wait") ||     // WAIT
               (op_name_str == "rlxwait")) {  // Relaxed WAIT
        /** Wait (i.e., semaphore acquire) operation */
        // {
        //   "name": "wait",
        //   "channel_ids": [
        //     0,
        //     1,
        //     2
        //   ],
        //   "channel_type": "memory"
        // }
        // Parse channel type
        if (j_op["channel_type"] != "memory") {
            THROW_ERR_NR("[Msccl::Parser] Unsupported channel type in"
                         " threadblock wait op: %s\n",
                         j_op["channel_type"].get<std::string>().c_str());
        }

        // Parse channel IDs and create Semaphore objects
        std::vector<Semaphore> sema_lst;
        for (const auto& chan_id : j_op["channel_ids"]) {
            auto cid = chan_id.get<uint32_t>();
            if (j_op["channel_type"] == "memory") {
                assert(cid < tb_mem_chan_lst.size());
                const auto& mem_chan = tb_mem_chan_lst[cid].get();
                assert(mem_chan.rank != gpu_parser.gpu_id);
                sema_lst.emplace_back(gpu_parser.gpu_id,  // this GPU ID
                                      mem_chan.rank,  // ID of the connected GPU
                                      mem_chan.chan_id,  // semaphore ID
                                      0);  // init value (always 0)
            }
        }
        assert(sema_lst.size() > 0);

        // Create SemAcquireOp
        auto op = std::make_shared<Msccl::SemAcquireOp>(
            op_name_str, sema_lst, BufferType::Semaphore,
            gpu_parser.wfreq_nbytes, niters);
        return op;
    } else if (op_name_str == "sem_acquire") {  // intra-gpu SEM_ACQUIRE
        // Parse semaphore IDs
        //  "semaphore_ids": [
        //    0
        //  ]
        std::vector<Semaphore> sema_lst;
        for (const auto& sem_id : j_op["semaphore_ids"]) {
            auto sid = sem_id.get<uint32_t>();
            assert(sid < gpu_parser.sema_lst.size());
            sema_lst.emplace_back(gpu_parser.gpu_id,  // this GPU ID
                                  gpu_parser.gpu_id,  // this GPU ID as well
                                  sid,                // semaphore ID
                                  0);                 // init value (always 0)
        }
        assert(sema_lst.size() > 0);

        // Create SemAcquireOp
        auto op = std::make_shared<Msccl::SemAcquireOp>(
            op_name_str, sema_lst, BufferType::Semaphore,
            gpu_parser.wfreq_nbytes, niters);
        return op;
    } else if (op_name_str == "sem_release") {  // intra-gpu SEM_RELEASE
        // Parse semaphore IDs
        //  "semaphore_ids": [
        //    0
        //  ]
        std::vector<Semaphore> sema_lst;
        for (const auto& sem_id : j_op["semaphore_ids"]) {
            auto sid = sem_id.get<uint32_t>();
            assert(sid < gpu_parser.sema_lst.size());
            sema_lst.emplace_back(gpu_parser.gpu_id,  // this GPU ID
                                  gpu_parser.gpu_id,  // this GPU ID as well
                                  sid,                // semaphore ID
                                  0);                 // init value (always 0)
        }
        assert(sema_lst.size() > 0);

        // Create SemReleaseOp
        auto op = std::make_shared<Msccl::SemReleaseOp>(
            op_name_str, sema_lst, BufferType::Semaphore,
            gpu_parser.wfreq_nbytes, niters);
        return op;
    } else if ((op_name_str == "put") ||     // PUT
               (op_name_str == "get") ||     // GET
               (op_name_str == "copy") ||    // COPY
               (op_name_str == "ppkt") ||    // PUT_PACKETS
               (op_name_str == "cpkt") ||    // COPY PACKETS
               (op_name_str == "upkt") ||    // UNPACK PACKETS
               (op_name_str == "rppkt") ||   // READ PUT PACKETS
               (op_name_str == "ruppkt")) {  // READ UNPACK PUT PACKETS
        /** MemCpy operation */
        std::vector<BufferChunk> src_buf_chunks;
        std::vector<BufferChunk> dst_buf_chunks;
        for (const auto& j_src_buf_chunk : j_op["src_buff"]) {
            src_buf_chunks.push_back(
                parseBufferChunk(j_src_buf_chunk, unit_chunk_cnt));
        }

        for (const auto& j_dst_buf_chunk : j_op["dst_buff"]) {
            dst_buf_chunks.push_back(
                parseBufferChunk(j_dst_buf_chunk, unit_chunk_cnt));
        }

        std::shared_ptr<MemCpyOp> op;
        if (gpu_parser.protocol == ProtocolType::Simple) {
            op = std::make_shared<Msccl::MemCpyOp>(
                op_name_str, src_buf_chunks, dst_buf_chunks,
                gpu_parser.unrolling_factor, gpu_parser.wfreq_nbytes, niters,
                iter_nbytes);
        } else {
            op = std::make_shared<Msccl::MemCpyLLOp>(
                op_name_str, src_buf_chunks, dst_buf_chunks,
                gpu_parser.unrolling_factor, gpu_parser.wfreq_nbytes, niters,
                iter_nbytes);
        }
        return op;
    } else if ((op_name_str == "re") ||    // REDUCE
               (op_name_str == "res") ||   // REDUCE SEND
               (op_name_str == "rre") ||   // READ REDUCE
               (op_name_str == "rres")) {  // READ REDUCE SEND
        /** Reduce operation */
        std::vector<BufferChunk> src_buf_chunks;
        for (const auto& j_src_buf_chunk : j_op["src_buff"]) {
            src_buf_chunks.push_back(
                parseBufferChunk(j_src_buf_chunk, unit_chunk_cnt));
        }
        std::vector<BufferChunk> dst_buf_chunks;
        for (const auto& j_dst_buf_chunk : j_op["dst_buff"]) {
            dst_buf_chunks.push_back(
                parseBufferChunk(j_dst_buf_chunk, unit_chunk_cnt));
        }
        auto op = std::make_shared<Msccl::ReduceOp>(
            op_name_str, src_buf_chunks, dst_buf_chunks,
            gpu_parser.unrolling_factor, gpu_parser.wfreq_nbytes, niters,
            iter_nbytes);
        return op;
    } else {
        THROW_ERR_NR("[Msccl::Parser] Unsupported operation name in"
                     " threadblock: %s\n",
                     op_name_str.c_str());
        return nullptr;
    }
}
