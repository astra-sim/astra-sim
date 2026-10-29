// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <astra-sim/system/gpu/msccl/Buffer.hh>
#include <astra-sim/system/gpu/msccl/Type.hh>

/**
 * Represents the memory of a GPU. Different GPU operations can modify the state
 * of the memory. At the end of the program, the state of memory of each GPU can
 * be observed and validated as being "correct" or "incorrect" compared to some
 * ideal memory state. Each GPU has exactly three buffer types, corresponding to
 * the Input, Output, and Scratch buffers. A buffer consists of a collection of
 * Cells.
 *
 * There are two kinds of memory buffer cells:
 *   1. Value-based cells. A cell is given an integer value, and operations will
 *      actually modify those values (i.e., copies will copy the value,
 *      reductions will add the values, etc.). Verification at the end of the
 *      program ensures all integer values are correct.
 *   2. Trace-based cells. Cells are functionally mnemonics characterized by
 *      their buffer position at the start of the program. Each operation
 *      performed on the buffers creates a *new* Cell which contains a
 *      description of the operation performed. For example, the buffer is
 *      initialized with MemoryCellInit objects, and a copy operation from one
 *      buffer position to another will replace the destination memory cell with
 *      a MemoryCellCopy object storing the source MemoryCellInit object as a
 *      pointer. During verification, we can either check for *semantic*
 *      equivalency (i.e., ensuring a destination memory cell contains the
 *      correct MemoryCellInit value(s), regardless of any redundant copies or
 *      being performed), or *complete* equivalency (i.e., also ensuring no
 *      redundant copies are performed. This mode is slower than value-based
 *      cells, but it gives maximum information about incorrect memory contents.
 */

namespace AstraSim {

class GPUModel;

/**
 * Needed to represent unordered_set's of two ints.
 */
struct PairHash {
    std::size_t operator()(
        const std::pair<uint32_t, uint32_t>& p) const noexcept {
        return (static_cast<std::size_t>(p.first) << 32) | p.second;
    }
};

/**
 * Used in later C++ versions, but manually implemented here with explicit
 * bounds checks. Represents a contiguous collection of data, defined only as a
 * starting and ending pointer.
 */
template <typename T> struct Span {
    Span() {}
    Span(T* _start_p, T* _end_p) : start_p(_start_p), end_p(_end_p) {}

    T& operator[](size_t index) const {
        assert(start_p + index < end_p);
        return start_p[index];
    }

    size_t size() const {
        return end_p - start_p;
    }

    /**
     * Returns true if this Span is inside of the other.
     */
    bool in(Span& other) const {
        return start_p >= other.start_p && end_p <= other.end_p;
    }

    T* start_p;
    T* end_p;
};

/**
 * A single cell of memory.
 */
struct MemoryCell {
    MemoryCell() = default;
    virtual ~MemoryCell() = default;

    /**
     * Returns true if this cell is semantically equivalent to the other.
     */
    virtual bool semanticallyEquivalent(const MemoryCell* other) const = 0;

    /**
     * Makes a copy of this cell and puts the reference into the destination.
     * This may or may not allocate new memory. `*dest` may be nullptr.
     */
    virtual void copy(MemoryCell** dest) const = 0;

    /**
     * Reduces ourselves into the destination cell. This may or may not allocate
     * new memory. `*dest` may be nullptr.
     */
    virtual void reduce(MemoryCell** dest) const = 0;

    /**
     * String representation of this cell.
     */
    virtual std::string toString() const = 0;
};

/**
 * A value-based memory cell. Contains an actual integer value with as little
 * metadata as possible.
 */
struct ValueMemoryCell : public MemoryCell {
    /**
     * Initializes this cell given the rank and buffer positions. These inputs
     * define the value.
     */
    ValueMemoryCell(uint64_t value);
    ValueMemoryCell(uint64_t rank_idx, uint64_t buffer_idx);
    ~ValueMemoryCell() = default;

    /**
     * Converts a rank index and buffer index into a value characterizing that
     * position.
     */
    static uint64_t getValue(uint64_t rank_idx, uint64_t buffer_idx);

    /**
     * Returns true if this cell has the same value as the other cell.
     */
    bool semanticallyEquivalent(const MemoryCell* other) const override;

    /**
     * Copes our value into the other cell. `*dest` must not be nullptr.
     */
    virtual void copy(MemoryCell** dest) const override;

    /**
     * Redues ourselves into the other cell. `*dest` must not be nullptr.
     */
    virtual void reduce(MemoryCell** dest) const override;

    /**
     * String representation of this cell.
     */
    virtual std::string toString() const override;

    /**
     * Integer value that this cell contains.
     */
    uint64_t value;
};

/**
 * A trace-based memory cell. Contains the full trace of operations which
 * happened on the relevant cells.
 */
struct TraceMemoryCell : public MemoryCell {
    TraceMemoryCell() = default;
    virtual ~TraceMemoryCell() = default;

    virtual void copy(MemoryCell** dest) const override = 0;
    virtual std::string toString() const override = 0;

    bool semanticallyEquivalent(const MemoryCell* other) const override;
    void reduce(MemoryCell** dest) const override;

    /**
     * Returns the collection of (gpu_id, chunk_id) MemoryCellInit cells which
     * make up this cell.
     */
    virtual std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash>
    baseCells() const = 0;

    /**
     * Converts ourselves into a value-based memory cell.
     */
    ValueMemoryCell* toValueCell() const;
};

/**
 * An initialized cell of memory.
 */
struct TraceMemoryCellInit : public TraceMemoryCell {
    TraceMemoryCellInit(uint32_t _gpu_id,
                        uint32_t _chunk_id,
                        Msccl::BufferType _type)
        : gpu_id(_gpu_id),
          chunk_id(_chunk_id),
          type(_type) {}
    ~TraceMemoryCellInit();

    std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash> baseCells()
        const override;
    void copy(MemoryCell** dest) const override;
    std::string toString() const override;

    /**
     * The ID of the GPU which contains this data.
     */
    uint32_t gpu_id;

    /**
     * The ID of the chunk this cell holds.
     */
    uint32_t chunk_id;

    /**
     * The type of the buffer (input, output, scratch).
     */
    Msccl::BufferType type;
};

/**
 * A cell of memory which was moved to another cell.
 */
struct TraceMemoryCellCopy : public TraceMemoryCell {
    TraceMemoryCellCopy(TraceMemoryCell* _original_cell,
                        uint32_t _new_gpu_id,
                        uint32_t _new_chunk_id)
        : original_cell(_original_cell),
          new_gpu_id(_new_gpu_id),
          new_chunk_id(_new_chunk_id) {}
    ~TraceMemoryCellCopy();

    std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash> baseCells()
        const override;
    void copy(MemoryCell** dest) const override;
    std::string toString() const override;

    /**
     * The cell we are a copy of.
     */
    TraceMemoryCell* original_cell;

    /**
     * The new ID of the GPU holding this cell.
     */
    uint32_t new_gpu_id;

    /**
     * The new ID of the chunk holding this cell.
     */
    uint32_t new_chunk_id;
};

/**
 * Combines a list of other memory cells together.
 */
struct TraceMemoryCellReduction : public TraceMemoryCell {
    TraceMemoryCellReduction(std::unordered_set<const TraceMemoryCell*> _cells)
        : cells(_cells) {}
    ~TraceMemoryCellReduction();

    std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash> baseCells()
        const override;
    void copy(MemoryCell** dest) const override;
    std::string toString() const override;

    std::unordered_set<const TraceMemoryCell*> cells;
};

/**
 * Represents the memory of a single GPU.
 */
class Memory {
  public:
    enum MemoryMode { Trace, Value };

    Memory(enum MemoryMode mode,
           uint32_t gpu_idx,
           bool inplace,
           std::unordered_map<Msccl::BufferType, uint32_t>& num_buf_chunks);
    ~Memory();

    /**
     * Sets the contents of the destination chunk to the source chunk.
     */
    void set(Memory* src_memory,
             Msccl::BufferChunk& src_chunk,
             Msccl::BufferChunk& dst_chunk);

    /**
     * Reduces the contents of the src chunk into the destination chunk.
     */
    void reduce(Memory* src_memory,
                Msccl::BufferChunk& src_chunk,
                Msccl::BufferChunk& dst_chunk);

    /**
     * Returns true if our layout is the same as the parameter, or false
     * otherwise.
     */
    bool sameLayout(
        std::unordered_map<Msccl::BufferType, uint32_t> num_buf_chunks,
        bool inplace);

    /**
     * Sets the expected memory layout to the parameter, which we will compare
     * against in the verification step.
     */
    void setExpected(std::unordered_map<uint32_t, const MemoryCell*>& expected);

    /**
     * Returns true if the memory matches expectations. Returns false and prints
     * error messages to stderr otherwise.
     */
    bool verify() const;

    /**
     * Returns a string representation of this object.
     */
    std::string toString() const;

  private:
    /**
     * Utility method for checking equivalence and printing errors due to
     * invalid chunks.
     */
    bool cell_check(const MemoryCell* actual_cell,
                    const MemoryCell* expected_cell,
                    uint32_t chunk_idx) const;

    /**
     * The way we're representing the memory.
     */
    enum MemoryMode m_mode;

    /**
     * The GPU index this memory resides on.
     */
    uint32_t m_gpu_idx;

    /**
     * The contiguous raw data this memory contains.
     */
    std::vector<MemoryCell*> memory;

    /**
     * Points each buffer type (input, output, scratch) to a region of the
     * underlying memory.
     */
    std::unordered_map<Msccl::BufferType, Span<MemoryCell*>> chunks;

    /**
     * The memory we expect to have at the end of the kernel.
     */
    std::unordered_map<uint32_t, const MemoryCell*> m_expected;
};

}  // namespace AstraSim
