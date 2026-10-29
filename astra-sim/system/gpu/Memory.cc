// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#include <cassert>
#include <iostream>
#include <limits>

#include <astra-sim/common/logging/EventTracer.hh>
#include <astra-sim/system/gpu/GPUModel.hh>
#include <astra-sim/system/gpu/Memory.hh>

namespace AstraSim {

ValueMemoryCell::ValueMemoryCell(uint64_t _value) : value(_value) {}

ValueMemoryCell::ValueMemoryCell(uint64_t rank_idx, uint64_t buffer_idx)
    : value(getValue(rank_idx, buffer_idx)) {}

uint64_t ValueMemoryCell::getValue(uint64_t rank_idx, uint64_t buffer_idx) {
    return (rank_idx << 32) | buffer_idx;
}

bool ValueMemoryCell::semanticallyEquivalent(const MemoryCell* other) const {
    const ValueMemoryCell* other_p =
        dynamic_cast<const ValueMemoryCell*>(other);
    assert(other_p);  // We shouldn't mix value and trace mode.
    return value == other_p->value;
}

void ValueMemoryCell::copy(MemoryCell** dest) const {
    assert(dest != nullptr);
    assert(*dest != nullptr);

    // Do not create a new cell, copy our value into the other cell.
    ValueMemoryCell* dest_p = dynamic_cast<ValueMemoryCell*>(*dest);
    assert(dest_p);
    dest_p->value = value;
}

void ValueMemoryCell::reduce(MemoryCell** dest) const {
    assert(dest != nullptr);
    assert(*dest != nullptr);

    // Do not create a new cell, add our value into the other cell.
    ValueMemoryCell* dest_p = dynamic_cast<ValueMemoryCell*>(*dest);
    assert(dest_p);
    dest_p->value += value;
}

std::string ValueMemoryCell::toString() const {
    return std::to_string(value);
}

void TraceMemoryCell::reduce(MemoryCell** dest) const {
    assert(dest != nullptr);
    assert(*dest != nullptr);  // We have to be reducing into something.

    // If the destination contains a reduction, then add ourselves into that
    // reduction. Otherwise, replace the destination with a reduction cell.
    TraceMemoryCellReduction* dest_reduction =
        dynamic_cast<TraceMemoryCellReduction*>(*dest);
    if (dest_reduction == nullptr) {
        // No need to make a copy; the destination will get overwritten later
        // and unusable in future operations.
        dest_reduction = new TraceMemoryCellReduction(
            {dynamic_cast<TraceMemoryCell*>(*dest)});
    }

    // Accumulate a copy of ourselves into the destination. We must create a
    // copy since we do not want future modifications of ourselves to affect
    // the destination.
    MemoryCell* this_copy = nullptr;
    this->copy(&this_copy);
    dest_reduction->cells.insert(dynamic_cast<TraceMemoryCell*>(this_copy));

    // Replace the destination with the new reduction, or replace it with itself
    // (which doesn't do anything).
    *dest = dest_reduction;
}

bool TraceMemoryCell::semanticallyEquivalent(const MemoryCell* other) const {
    const TraceMemoryCell* trace = dynamic_cast<const TraceMemoryCell*>(other);
    assert(trace);  // Memory cell types can't be combined.

    std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash>
        our_cells = baseCells(),
        their_cells = trace->baseCells();

    // Must contain the same contents.
    if (our_cells.size() != their_cells.size()) {
        return false;
    }
    for (const std::pair<uint32_t, uint32_t> chunks : their_cells) {
        if (our_cells.count(chunks) != 1) {
            return false;
        }
    }
    return true;
}

ValueMemoryCell* TraceMemoryCell::toValueCell() const {
    uint64_t value = 0;
    for (const auto& it : baseCells()) {
        uint32_t gpu_id = it.first;
        uint32_t chunk_id = it.second;
        value += ValueMemoryCell::getValue(gpu_id, chunk_id);
    }
    return new ValueMemoryCell(value);
}

TraceMemoryCellInit::~TraceMemoryCellInit() {
    // Nothing dynamically allocated here.
}

void TraceMemoryCellInit::copy(MemoryCell** dest) const {
    assert(dest != nullptr);  // Though *dest may be nullptr.

    // Create a new cell containing the same data as ourselves.
    MemoryCell* new_cell = new TraceMemoryCellInit(gpu_id, chunk_id, type);

    // Free whatever's in the cell we're about to overwrite, since it can't be
    // accessed anymore after our overwrite.
    if (*dest != nullptr) {
        delete *dest;
    }
    *dest = new_cell;  // Completely overwrite original cell.
}

std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash> TraceMemoryCellInit::
    baseCells() const {
    return {{gpu_id, chunk_id}};
}

std::string TraceMemoryCellInit::toString() const {
    return std::to_string(gpu_id) + "[" + std::to_string(chunk_id) + "]";
}

TraceMemoryCellCopy::~TraceMemoryCellCopy() {
    // The original cell is dynamically allocated.
    delete original_cell;
}

void TraceMemoryCellCopy::copy(MemoryCell** dest) const {
    assert(dest != nullptr);  // Though *dest may be nullptr.

    // Create a new cell representing a movement of data from the original cell.
    // Create a copy of our original cell so future modifications of it does not
    // impact this destination cell.
    MemoryCell* this_copy = nullptr;
    original_cell->copy(&this_copy);
    MemoryCell* new_cell = new TraceMemoryCellCopy(
        dynamic_cast<TraceMemoryCell*>(this_copy), new_gpu_id, new_chunk_id);

    // Free whatever's in the cell we're about to overwrite, since it can't be
    // accessed anymore after our overwrite.
    if (*dest != nullptr) {
        delete *dest;
    }
    *dest = new_cell;  // Completely overwrite original cell.
}

std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash> TraceMemoryCellCopy::
    baseCells() const {
    return original_cell->baseCells();
}

std::string TraceMemoryCellCopy::toString() const {
    return original_cell->toString();
}

TraceMemoryCellReduction::~TraceMemoryCellReduction() {
    // Each of the reduced cells are dynamically allocated.
    for (const TraceMemoryCell* cell : cells) {
        delete cell;
    }
}

void TraceMemoryCellReduction::copy(MemoryCell** dest) const {
    assert(dest != nullptr);  // Though *dest may be nullptr.

    // Create a new cell representing a reduction of data from ourselves into
    // the destination. Create a copy of ourselves so future modifications of
    // ourselves does not impact this destination cell.
    std::unordered_set<const TraceMemoryCell*> cells_copy(cells.size());
    for (const TraceMemoryCell* const& cell : cells) {
        // Create a copy of the input.
        MemoryCell* cell_copy = nullptr;
        cell->copy(&cell_copy);
        cells_copy.insert(dynamic_cast<TraceMemoryCell*>(cell_copy));
    }
    MemoryCell* new_cell = new TraceMemoryCellReduction(cells_copy);

    // Free whatever's in the cell we're about to overwrite, since it can't be
    // accessed anymore after our overwrite.
    if (*dest != nullptr) {
        delete *dest;
    }
    *dest = new_cell;  // Completely overwrite original cell.
}

std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash>
TraceMemoryCellReduction::baseCells() const {
    std::unordered_set<std::pair<uint32_t, uint32_t>, PairHash> result;
    for (auto& it : cells) {
        for (auto& base : it->baseCells()) {
            result.insert(base);
        }
    }
    return result;
}

std::string TraceMemoryCellReduction::toString() const {
    std::ostringstream oss;
    bool first = true;
    for (auto& it : baseCells()) {
        if (!first) {
            oss << " + ";
        } else {
            first = false;
        }
        oss << it.first << "[" << it.second << "]";
    }
    return oss.str();
}

Memory::Memory(enum MemoryMode mode,
               uint32_t gpu_idx,
               bool inplace,
               std::unordered_map<Msccl::BufferType, uint32_t>& num_buf_chunks)
    : m_mode(mode),
      m_gpu_idx(gpu_idx) {
    // There should be exactly three categories of chunks passed in:
    // Scratch, Input, and Output.
    assert(num_buf_chunks.size() == 3);

    // First, determine how many chunks to allocate.
    unsigned num_scratch = num_buf_chunks[Msccl::BufferType::Scratch];
    unsigned num_input = num_buf_chunks[Msccl::BufferType::Input];
    unsigned num_output = num_buf_chunks[Msccl::BufferType::Output];

    // If it's inplace, the input/output region is shared.
    int num_chunks = num_scratch + (inplace ? std::max(num_input, num_output)
                                            : num_input + num_output);

    // Allocate this space, then point the chunks into this allocated region.
    // Overwrite the chunk indices later.
    memory.reserve(num_chunks);
    for (unsigned idx = 0; idx < num_chunks; idx++) {
        MemoryCell* cell;
        if (mode == MemoryMode::Trace) {
            cell = new TraceMemoryCellInit(gpu_idx,
                                           std::numeric_limits<uint32_t>::max(),
                                           Msccl::BufferType::Scratch);
        } else {
            cell = new ValueMemoryCell(gpu_idx,
                                       std::numeric_limits<uint32_t>::max());
        }
        memory.push_back(cell);
    }
    MemoryCell** data = memory.data();

    // Scratch chunks go first.
    chunks[Msccl::BufferType::Scratch] =
        Span<MemoryCell*>(data + 0, data + num_scratch);

    if (!inplace) {
        // Input and output go next.
        chunks[Msccl::BufferType::Input] = Span<MemoryCell*>(
            data + num_scratch, data + num_scratch + num_input);
        chunks[Msccl::BufferType::Output] = Span<MemoryCell*>(
            data + num_scratch + num_input, data + num_chunks);
    } else {
        // The larger space takes up the remaining area, and the remaining space
        // takes up area according to a strided pattern based on how many GPUs
        // there are.
        Msccl::BufferType larger =
            (num_input > num_output ? Msccl::BufferType::Input
                                    : Msccl::BufferType::Output);
        Msccl::BufferType smaller =
            (larger == Msccl::BufferType::Input ? Msccl::BufferType::Output
                                                : Msccl::BufferType::Input);

        unsigned stride = (num_buf_chunks[smaller] != num_buf_chunks[larger])
                              ? num_buf_chunks[smaller]
                              : 0;
        assert(num_scratch + stride * gpu_idx < num_chunks);
        chunks[larger] =
            Span<MemoryCell*>(data + num_scratch, data + num_chunks);
        chunks[smaller] = Span<MemoryCell*>(
            data + num_scratch + stride * gpu_idx,
            data + num_scratch + stride * gpu_idx + num_buf_chunks[smaller]);
    }

    // For each chunk area, set the chunk indices correctly. In case the input
    // and output overlap, ensure the input chunk indices are written last.
    for (Msccl::BufferType buf_type :
         {Msccl::BufferType::Scratch, Msccl::BufferType::Output,
          Msccl::BufferType::Input}) {
        Span<MemoryCell*> span = chunks.at(buf_type);
        for (size_t idx = 0; idx < span.size(); idx++) {
            if (m_mode == MemoryMode::Trace) {
                TraceMemoryCellInit* cell =
                    dynamic_cast<TraceMemoryCellInit*>(&*span[idx]);
                cell->chunk_id = idx;
                cell->type = buf_type;
            } else {  // m_mode == MemoryMode::Value
                ValueMemoryCell* cell =
                    dynamic_cast<ValueMemoryCell*>(&*span[idx]);
                cell->value = ValueMemoryCell::getValue(gpu_idx, idx);
            }
        }
    }

    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, uint32_t gpu_idx, unsigned num_scratch,
           unsigned num_input, unsigned num_output) {
            os << "Memory: creating memory for gpu " << gpu_idx << ": "
               << "Input = " << num_input << ", Output = " << num_output
               << ", Scratch = " << num_scratch;
        },
        m_gpu_idx, num_scratch, num_input, num_output);
}

Memory::~Memory() {
    // Free each pointer we allocated.
    for (auto& it : m_expected) {
        delete it.second;
    }
    for (MemoryCell*& cell : memory) {
        delete cell;
    }
}

void Memory::set(Memory* src_memory,
                 Msccl::BufferChunk& src,
                 Msccl::BufferChunk& dst) {
    assert(m_gpu_idx == dst.rank);
    assert(src_memory->m_gpu_idx == src.rank);

    // While this isn't strictly incorrect, if this happens, it probably
    // indicates a bug.
    assert(src != dst);

    // Get pointers to input and output. We will be overwriting the output.
    MemoryCell* src_ptr =
        src_memory->chunks[src.type][src.byte_offset / src.nbytes];
    MemoryCell*& dst_ptr = chunks[dst.type][dst.byte_offset / dst.nbytes];

    // If we're using a trace-based memory mode, then represent the copy by a
    // mnemonic. Otherwise, perform the copy directly.
    if (m_mode == MemoryMode::Trace) {
        // Make a copy of the source so future modifications of it do not impact
        // the new destination.
        MemoryCell* src_copy = nullptr;
        src_ptr->copy(&src_copy);
        TraceMemoryCellCopy* new_p =
            new TraceMemoryCellCopy(dynamic_cast<TraceMemoryCell*>(src_copy),
                                    dst.rank, dst.byte_offset / dst.nbytes);
        dst_ptr = new_p;  // Overwrite reference
    } else {              // m_mode == MemoryMode::Value
        src_ptr->copy(&dst_ptr);
    }

    // Print logs.
    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const Memory& memory,
           const Msccl::BufferChunk& src, const Msccl::BufferChunk& dst) {
            os << "Memory::set("
               << "src = " << src.to_string() << ", "
               << "dst = " << dst.to_string() << ")" << std::endl
               << memory.toString();
        },
        *this, src, dst);
}

void Memory::reduce(Memory* src_memory,
                    Msccl::BufferChunk& src,
                    Msccl::BufferChunk& dst) {
    assert(m_gpu_idx == dst.rank);
    assert(src_memory->m_gpu_idx == src.rank);

    // While this isn't strictly incorrect, if this happens, it probably
    // indicates a bug.
    assert(src != dst);

    // Get pointers to input and output. We will be overwriting the output.
    MemoryCell* src_ptr =
        src_memory->chunks[src.type][src.byte_offset / src.nbytes];
    MemoryCell** dst_ptr = &(chunks[dst.type][dst.byte_offset / dst.nbytes]);

    // Directly reduce the input into the output.
    src_ptr->reduce(dst_ptr);

    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const Memory& memory,
           const Msccl::BufferChunk& src, const Msccl::BufferChunk& dst) {
            os << "Memory::reduce("
               << "src = " << src.to_string() << ", "
               << "dst = " << dst.to_string() << ")" << std::endl
               << memory.toString();
        },
        *this, src, dst);
}

bool Memory::sameLayout(
    std::unordered_map<Msccl::BufferType, uint32_t> num_buf_chunks,
    bool inplace) {
    assert(num_buf_chunks.size() == 3);
    assert(chunks.size() == 3);

    Span<MemoryCell*>&input_span = chunks.at(Msccl::BufferType::Input),
    &output_span = chunks.at(Msccl::BufferType::Output),
    &scratch_span = chunks.at(Msccl::BufferType::Scratch);
    assert(inplace ==
           (input_span.in(output_span) || output_span.in(input_span)));

    return (num_buf_chunks[Msccl::BufferType::Scratch] ==
                chunks.at(Msccl::BufferType::Scratch).size() &&
            num_buf_chunks[Msccl::BufferType::Input] ==
                chunks.at(Msccl::BufferType::Input).size() &&
            num_buf_chunks[Msccl::BufferType::Output] ==
                chunks.at(Msccl::BufferType::Output).size());
}

bool Memory::cell_check(const MemoryCell* actual_cell,
                        const MemoryCell* expected_cell,
                        uint32_t chunk_idx) const {
    bool matched;
    if (actual_cell->semanticallyEquivalent(expected_cell)) {
        matched = true;
    } else {
        std::cerr << "GPU " << m_gpu_idx << ", chunk " << chunk_idx
                  << ": Received " << actual_cell->toString() << ", expected "
                  << expected_cell->toString() << std::endl;
        matched = false;
    }

    g_functional_tracer.trace(
        nullptr,
        [](std::ofstream& os, const Memory& memory,
           const MemoryCell* actual_cell, uint32_t chunk_idx, bool matched) {
            if (matched) {
                os << "[equals] ";
            } else {
                os << "[   not] ";
            }
            os << "GPU " << memory.m_gpu_idx << ", chunk " << chunk_idx << ": "
               << actual_cell->toString();
        },
        *this, actual_cell, chunk_idx, matched);

    return matched;
}

void Memory::setExpected(
    std::unordered_map<uint32_t, const MemoryCell*>& expected) {
    // If the memory mode is value-based and the cells are trace-based cells,
    // then convert them.
    m_expected.reserve(expected.size());
    for (const auto& it : expected) {
        uint32_t idx = it.first;
        const MemoryCell* cell = it.second;

        const TraceMemoryCell* trace_cell =
            dynamic_cast<const TraceMemoryCell*>(cell);
        const ValueMemoryCell* value_cell =
            dynamic_cast<const ValueMemoryCell*>(cell);

        assert((trace_cell != nullptr) ^ (value_cell != nullptr));
        if (m_mode == MemoryMode::Value && trace_cell != nullptr) {
            // Convert cell to value mode.
            const MemoryCell* temp = trace_cell->toValueCell();
            delete cell;
            expected[it.first] = temp;
            cell = temp;
        } else if (m_mode == MemoryMode::Trace && value_cell != nullptr) {
            // We don't support casting value-based cells into trace-based
            // cells. Change the code so that the caller of this function passes
            // a trace-based cell.
            assert(0);
        }
        m_expected[idx] = cell;
    }
}

bool Memory::verify() const {
    bool all_match = true;
    const Span<MemoryCell*>& buffer = chunks.at(Msccl::BufferType::Output);
    for (const auto& it : m_expected) {
        uint32_t idx = it.first;
        const MemoryCell* cell = it.second;
        all_match = cell_check(buffer[idx], cell, idx) && all_match;
    }
    return all_match;
}

std::string Memory::toString() const {
    const Span<MemoryCell*>&input_span = chunks.at(Msccl::BufferType::Input),
          &output_span = chunks.at(Msccl::BufferType::Output),
          &scratch_span = chunks.at(Msccl::BufferType::Scratch);

    std::ostringstream oss;
    oss << "Memory(GPU = " << m_gpu_idx << "):" << std::endl;

    oss << "  Scratch: ";
    for (size_t idx = 0; idx < scratch_span.size(); idx++) {
        oss << "    " << idx << ".) " << scratch_span[idx]->toString();
    }
    oss << std::endl;

    oss << "  Input:   ";
    for (size_t idx = 0; idx < input_span.size(); idx++) {
        oss << "    " << idx << ".) " << input_span[idx]->toString();
    }
    oss << std::endl;

    oss << "  Output:  ";
    for (size_t idx = 0; idx < output_span.size(); idx++) {
        oss << "    " << idx << ".) " << output_span[idx]->toString();
    }

    return oss.str();
}

}  // namespace AstraSim
