NS-3 Same-Key Receive Queue Regression Test
Ref PR: https://github.com/astra-sim/astra-sim/pull/383

BINARY:
    NS-3 AstraSimNetwork backend.

INPUTS:
    WORKLOAD:
        Four generated Chakra traces. Ranks 0 and 1 each send two messages to
        ranks 2 and 3, respectively. Both messages use the same tag, source,
        and destination but have different sizes. The two receive nodes on
        each destination are dependency-free and are posted before the first
        cross-datacenter message arrives.
    SYSTEM:
        Native point-to-point communication.
    NETWORK:
        Four endpoints connected through two datacenter switches. Endpoint
        links are 400 Gbps with 0.005 ms latency; the inter-datacenter path is
        10 Gbps with 30 ms latency on each side.
    MEMORY:
        No remote memory expansion.

VALIDATION:
    The run must finish within 30 seconds, all four ranks must report
    completion, and both receive-node callbacks must appear for ranks 2 and 3.
    Before the receive queue fix, the second same-key receive replaced the
    first receive's callback. The first node remained ongoing and this test
    timed out.
