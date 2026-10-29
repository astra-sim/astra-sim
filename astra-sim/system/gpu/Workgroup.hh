// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

#pragma once

#include <unordered_set>

#include <astra-sim/system/gpu/GpuOp.hh>
#include <astra-sim/system/gpu/Wavefront.hh>

/**
 * Workgroup representing a collection of wavefronts performing the same
 * sequence of GPU operations
 */

namespace AstraSim {

class ComputeUnit;

class Workgroup {
  public:
    Workgroup(uint32_t gpu_id,
              uint32_t kernel_id,
              uint32_t wg_id,
              GpuOpPtr first_wg_op_p);
    virtual ~Workgroup() = default;

    /** Getters */
    uint32_t getGpuId() const;
    uint32_t getKernelId() const;
    uint32_t getWorkgroupId() const;

    // Executes this workgroup. This may either lead to the completion of the
    // workgroup, or it may lead to a stall that must be executed again later.
    virtual void execute();

    // Returns true if this workgroup is done.
    virtual bool isCompleted() const;

    // Return a string representation of this workgroup
    virtual std::string to_string() const;

  protected:
    const uint32_t m_gpu_id;     // GPU ID
    const uint32_t m_kernel_id;  // Node ID representing kernel launch ID
    const uint32_t m_wg_id;      // Workgroup ID
    GpuOpPtr m_gpu_op;           // Current op we're executing
};

class TimingWorkgroup : public Workgroup {
  public:
    TimingWorkgroup(uint32_t gpu_id,
                    uint32_t kernel_id,
                    uint32_t wg_id,
                    uint32_t num_wfs,
                    GpuOpPtr first_wg_op_p,
                    ComputeUnit* cu_p);
    ~TimingWorkgroup() = default;

    /** Getters **/
    uint32_t getNumWavefronts() const;
    uint32_t getCuId() const;
    uint32_t getNumOutstandingWavefronts() const;

    // Creates the wavefront requests.
    void execute() override;

    // Return true if all wavefronts in this workgroups are completed
    bool isCompleted() const override;

    // Execute NoOp operation for the given wavefront
    void execNopOp(Wavefront* wf_p);

    // Execute Barrier operation for the given wavefront
    void execBarrierOp(Wavefront* wf_p,
                       uint32_t barrier_id,
                       uint32_t num_wgs_in_barrier);

    // Complete a wavefront
    void completeWavefront(uint32_t wf_id);

    // Unstall after a Nop or Barrier operation is done
    void unstall();

    // Return pointer to the owning compute unit
    ComputeUnit* getComputeUnitPtr() const;

    // Return pointer to the wavefront with the given ID
    Wavefront* getWavefrontPtr(uint32_t wf_id);

    std::string to_string() const override;

  private:
    bool m_is_executed;        // True if we began execution
    const uint32_t m_num_wfs;  // Number of wavefronts in this workgroup
    ComputeUnit* m_cu_p;       // Pointer to the CU processing this workgroup
    std::vector<Wavefront> m_wf_lst;  // List of wavefronts of this workgroup
    uint32_t m_num_outstanding_wfs;   // Number of outstanding wavefronts
    std::unordered_set<uint32_t> m_stalled_wf_ids;
};

}  // namespace AstraSim
