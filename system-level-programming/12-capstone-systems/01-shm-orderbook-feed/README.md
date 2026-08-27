# Capstone 01: Ultra-Low Latency L2 Shared-Memory Orderbook

A production-grade, dual-process High-Frequency Trading (HFT) market data pipeline and Level 2 (L2) Limit Order Book matching engine.

---

## 🏛️ System Architecture

```
+-------------------------------------------------------------------------------+
| PROCESS 1: Exchange Market Data Simulator (Producer)                          |
|                                                                               |
|  Generates Add/Cancel/Trade events -> Publishes to POSIX Shared Memory Ring   |
+---------------------------------------|---------------------------------------+
                                        v (/dev/shm/hft_orderbook_feed)
+-------------------------------------------------------------------------------+
| ZERO-COPY POSIX SHARED MEMORY FEED                                            |
|                                                                               |
|  * Wait-Free SPSC Circular Ring Buffer                                        |
|  * 64-Byte Cacheline Aligned Event Structs (MarketEvent)                      |
|  * Sub-100ns Cross-Process Delivery                                           |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| PROCESS 2: Algorithmic Trading Engine (Consumer)                              |
|                                                                               |
|  1. Consumes MarketEvent from SHM                                             |
|  2. Updates Cacheline-Aligned L2 Limit Order Book (Bids & Asks)               |
|  3. Computes Mid-Price, Micro-Price & Generates Trading Signal                |
|  4. Records End-to-End Tick-to-Trade Latency with Hardware TSC                |
+-------------------------------------------------------------------------------+
```

---

## 🔬 Level 2 Limit Order Book Mechanics

- **Fixed Depth Arrays**: Bids and Asks are maintained in statically allocated arrays sorted by price-time priority.
- **Top of Book**: Best Bid (Highest Buy Price) and Best Ask (Lowest Sell Price) accessible in $O(1)$.
- **Zero Allocations**: Zero heap allocations on the hotpath (`no malloc/new`).
