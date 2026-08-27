# Module 07: Database Storage Engine & Buffer Pool Management

An exhaustive, CMU 15-445 / 15-721 standard database kernel reference on disk storage architecture, physical page layout, slotted pages, tuple serialization, buffer pool management, frame tables, and the LRU-K replacement algorithm.

---

## 1. Physical Page Layout & Slotted Page Architecture

Disk drives and NVMe SSDs transfer data in fixed-size blocks (typically $4\text{ KB}$ or $8\text{ KB}$). Database storage engines organize raw disk blocks into uniform **Database Pages**.

```
+---------------------------------------------------------------------------------------+
| SLOTTED PAGE PHYSICAL LAYOUT (4096 Bytes / 4 KB)                                     |
|                                                                                       |
| +-----------------------------------------------------------------------------------+ |
| | PAGE HEADER (24 Bytes)                                                            | |
| |  - page_id_t page_id       (8B) : Unique persistent disk page identifier          | |
| |  - lsn_t     page_lsn      (8B) : Log Sequence Number of last modifying write      | |
| |  - uint16_t  slot_count    (2B) : Number of allocated tuple slots                  | |
| |  - uint16_t  free_space_ptr(2B) : Byte offset to start of free space              | |
| |  - uint32_t  flags         (4B) : Page status flags                               | |
| +-----------------------------------------------------------------------------------+ |
| | SLOT ARRAY (Grows FORWARD ->)                                                     | |
| |  [Slot 0: offset=3900, size=196, deleted=0]                                       | |
| |  [Slot 1: offset=3750, size=150, deleted=0]                                       | |
| |  [Slot 2: offset=0,    size=0,   deleted=1] (Tombstone)                           | |
| |  [Slot 3: offset=3500, size=250, deleted=0]                                       | |
| +-----------------------------------------------------------------------------------+ |
| |                                                                                   | |
| |                       >>> UNALLOCATED FREE SPACE <<<                              | |
| |                                                                                   | |
| +-----------------------------------------------------------------------------------+ |
| | TUPLE STORAGE AREA (Grows BACKWARD <- from Offset 4096)                           | |
| |                                                                                   | |
| |  ... [Tuple 3: 250 Bytes] ... [Tuple 1: 150 Bytes] ... [Tuple 0: 196 Bytes]       | |
| +-----------------------------------------------------------------------------------+ |
+---------------------------------------------------------------------------------------+
```

### Why Slotted Pages?
1. **Variable-Length Tuples**: Tuples can contain dynamic string or binary columns (`VARCHAR`, `TEXT`, `BLOB`).
2. **Stable Record IDs (RID)**: An RID is a `(page_id, slot_id)` tuple. If a tuple is updated and expands, or if the page is compacted, only the slot's `offset` changes. External indexes pointing to `(page_id, slot_id)` remain valid without expensive index rewrites!
3. **In-Place Compaction**: Deleting tuples creates internal fragmentation. Compaction slides surviving tuples to the end of the page and updates slot offsets in a single $O(N)$ linear pass.

---

## 2. Buffer Pool Subsystem Architecture

The **Buffer Pool Manager (BPM)** is the core memory coordinator mediating between volatile DRAM frames and persistent disk storage.

```
+-------------------------------------------------------------------------------+
| EXECUTION ENGINE / QUERY PROCESSOR (Fetches Pages by page_id)                 |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| BUFFER POOL MANAGER (BPM)                                                     |
|                                                                               |
|  Page Table (Hash Table / Bucket Latches): page_id_t -> frame_id_t            |
|                                                                               |
|  Frame Table Metadata:                                                        |
|  +----------+----------+------------+-----------+---------------------------+ |
|  | Frame ID | Page ID  | Pin Count  | Dirty Bit | LRU-K History Timestamps  | |
|  +----------+----------+------------+-----------+---------------------------+ |
|  | 0        | 1042     | 2 (In Use) | 1 (Dirty) | [t1, t2]                  | |
|  | 1        | 5001     | 0 (Victim) | 0 (Clean) | [t3, t4] (Evictable)      | |
|  | ...      | ...      | ...        | ...       | ...                       | |
|  +----------+----------+------------+-----------+---------------------------+ |
+-------------------|-----------------------------------|-----------------------+
                    v (Cache Miss / Eviction)           v (Dirty Page Flush)
+-------------------------------------------------------------------------------+
| DISK STORAGE SUBSYSTEM (DiskManager)                                          |
|                                                                               |
|  Physical DB File: [Page 0] [Page 1] ... [Page 1042] ... [Page N]             |
+-------------------------------------------------------------------------------+
```

---

## 3. Buffer Replacement Policies

### A. Clock-Sweep (Second-Chance)
- Circular array with a hand pointer. Each frame has a 1-bit reference counter.
- When seeking a victim, if `ref_bit == 1`, clear to `0` and advance hand. If `ref_bit == 0` and `pin_count == 0`, evict frame.

### B. LRU-K ($k=2$) Algorithm
- Tracks the timestamps of the last $K$ accesses for each frame:
  $$\text{Backward } k\text{-Distance} = t_{\text{current}} - t_{\text{access}}(current - k)$$
- Frames with $< K$ accesses have an **infinite backward distance** ($\infty$), prioritized for eviction in FIFO order of their earliest access.
- **Sequential Scan Resistance**: A full table scan accessing thousands of pages once will not evict hot, frequently accessed index pages!

---

## 4. Buffer Allocation & Writeback Policies

| Policy | Option | Description | Tradeoff |
|---|---|---|---|
| **STEAL** | `STEAL` | Uncommitted transactions can write dirty pages to disk to free buffer frames. | High buffer flexibility; requires **UNDO logging** during recovery. |
| | `NO-STEAL` | Dirty pages of active transactions cannot be flushed until `COMMIT`. | Simple recovery; buffer pool quickly runs out of frames on large transactions. |
| **FORCE** | `FORCE` | All dirty pages modified by a transaction must be flushed to disk at `COMMIT`. | Simple recovery; terrible random I/O write bottleneck on commit. |
| | `NO-FORCE` | Transactions commit once WAL log is flushed; dirty data pages flush lazily. | **Industry Standard (Fast Commit)**; requires **REDO logging** during recovery. |

---

## 💻 Lab Exercises & Projects in this Module

1. **`exercises/slotted_page.hpp` & `exercises/slotted_page_layout.cpp`**:
   - Complete 4KB slotted page serialization engine supporting variable-length tuple insertion, updates, deletions, and in-place defragmenting compaction.
2. **`tests/test_slotted_page.cpp`**:
   - Unit tests validating slotted page boundary conditions and fragmentation recovery.
3. **`projects/concurrent_buffer_pool_manager/`**:
   - Thread-safe **Disk-Backed Buffer Pool Manager** featuring:
     - `LRUKReplacer` ($k=2$ backward distance eviction).
     - Bucket-latched Page Table (`std::shared_mutex`).
     - Multi-threaded concurrency stress test verifying pin count invariants and zero memory leaks.
