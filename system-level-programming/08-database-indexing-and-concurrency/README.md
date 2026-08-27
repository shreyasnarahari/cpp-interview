# Module 08: Database Indexing & Index Concurrency

An exhaustive database systems engineering reference on $B^+$ Tree index physical structures, latch crabbing/coupling concurrency protocols, optimistic lock coupling, and Log-Structured Merge-Tree (LSM-Tree) architectures.

---

## 1. $B^+$ Tree Node Physical Organization

A $B^+$ Tree is an $M$-way self-balancing search tree where all data tuples or Record IDs (RIDs) reside strictly in **Leaf Nodes**, linked sequentially for fast range scans.

```
                         +-----------------------------+
                         | Root / Internal Node (Page) |
                         | [Ptr0] K1 [Ptr1] K2 [Ptr2]  |
                         +-------/-------------\-------+
                                /               \
         +-----------------------+             +-----------------------+
         | Internal Node (Page)  |             | Internal Node (Page)  |
         | [Ptr0] K3 [Ptr1] K4   |             | [Ptr0] K5 [Ptr1] K6   |
         +-------/-------\-------+             +-------/-------\-------+
                /         \                           /         \
    +------------+       +------------+   +------------+       +------------+
    | Leaf Node  |------>| Leaf Node  |-->| Leaf Node  |------>| Leaf Node  |
    | [K1:RID1]  |       | [K3:RID3]  |   | [K5:RID5]  |       | [K7:RID7]  |
    +------------+       +------------+   +------------+       +------------+
```

### Internal Nodes vs Leaf Nodes:
- **Internal Nodes**: Store $(K-1)$ search keys and $K$ page pointers. Keys guide binary search navigation down the tree.
- **Leaf Nodes**: Store sorted `(Key, Value/RID)` pairs alongside sibling pointers `next_page_id` and `prev_page_id`.

---

## 2. Index Concurrency Control: Latch Crabbing / Coupling

To allow hundreds of concurrent threads to read and write $B^+$ Tree indexes simultaneously without corrupting node layouts:

```
Search (Read Latch Crabbing):
  1. Acquire Read Latch (R) on Root.
  2. Locate child pointer.
  3. Acquire Read Latch (R) on Child.
  4. Release Read Latch (R) on Root/Parent.
  5. Repeat until reaching Leaf Node.
```

```
Insert / Delete (Write Latch Crabbing):
  1. Acquire Write Latch (W) on Root.
  2. Check if Child is "Safe" (For Insert: has free slot < MAX; For Delete: has keys > MIN).
  3. Acquire Write Latch (W) on Child.
  4. If Child is SAFE:
       Release ALL held ancestor Write Latches!
  5. If Child is UNSAFE (may split/merge):
       Retain parent Write Latch and continue downward.
```

---

## 3. Log-Structured Merge-Tree (LSM-Tree) Architecture

For write-heavy database workloads (e.g. RocksDB, Cassandra, LevelDB), random disk writes in $B^+$ Trees cause severe write amplification. LSM-Trees convert random writes into **sequential append-only writes**.

```
+-------------------------------------------------------------------------------+
| INCOMING WRITES (Put / Delete)                                                |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| MEMORY LAYER: Concurrent MemTable (SkipList) + Write-Ahead Log (WAL)          |
|                                                                               |
|  * Fast in-memory sorted insertion via CAS ($O(\log N)$)                     |
|  * Persistent append-only WAL on disk                                         |
+---------------------------------------|---------------------------------------+
                                        v (When MemTable reaches threshold, e.g. 64MB)
+-------------------------------------------------------------------------------+
| FLUSH WORKER: Flushes Immutable MemTable to Level 0 SSTables (Disk)           |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| DISK STORAGE: Sorted String Tables (SSTables)                                 |
|                                                                               |
|  Level 0: [SSTable 0] [SSTable 1] [SSTable 2] (Overlapping Key Ranges)        |
|                  |                                                            |
|                  v (Background Leveled / Tiered Compaction)                   |
|  Level 1: [SSTable A (0-100)] [SSTable B (101-200)] [SSTable C (201-300)]    |
+-------------------------------------------------------------------------------+
```

### SSTable Internal Structure:
1. **Data Blocks**: 4KB contiguous sorted key-value pairs.
2. **Index Block**: Sparse binary search index storing the last key of each 4KB data block and its byte offset.
3. **Bloom Filter**: In-memory bit array with $K$ hash functions. Eliminates $\sim 99\%$ of negative disk reads ($O(1)$ pre-check).

---

## 💻 Lab Exercises & Projects in this Module

1. **`exercises/latch_crabbing_btree.hpp` & `exercises/latch_crabbing_btree.cpp`**:
   - Thread-safe in-memory $B^+$ Tree implementing exact Root-to-Leaf Read and Write Latch Crabbing protocols.
2. **`tests/test_latch_crabbing.cpp`**:
   - Multi-threaded concurrent insertion and search stress tests verifying absence of race conditions and deadlocks.
3. **`projects/concurrent_lsm_tree_engine/`**:
   - Production embedded **LSM-Tree Key-Value Engine**:
     - `bloom_filter.hpp`: High-performance optimal Bloom Filter.
     - `skiplist_memtable.hpp`: Concurrent SkipList MemTable.
     - `sstable.hpp`: Binary search block-indexed SSTable builder and reader.
     - `lsm_engine.hpp`: Integrated engine with automatic flush and multi-version merge lookups.
