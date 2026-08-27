# Capstone 02: Production Embedded LSM-Tree Key-Value Database

An industrial-grade, crash-resilient Embedded Key-Value Storage Engine combining a Concurrent SkipList MemTable, Write-Ahead Logging (WAL), immutable binary-indexed SSTables with Bloom filters, background multi-level compaction, and Snapshot Isolation queries.

---

## 🏛️ System Architecture

```
+-------------------------------------------------------------------------------+
| CLIENT APPLICATION (Put / Get / Delete / Range Scan)                          |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| EMBEDDED LSM STORAGE ENGINE                                                   |
|                                                                               |
|  1. WAL Logger: Sequential append-only crash durability                       |
|  2. Active MemTable: Concurrent SkipList ($O(\log N)$ in-memory mutations)     |
|  3. Read-View Snapshot: MVCC version visibility                               |
+---------------------------------------|---------------------------------------+
                                        v (When MemTable reaches threshold)
+-------------------------------------------------------------------------------+
| FLUSH WORKER & BACKGROUND COMPACTOR                                           |
|                                                                               |
|  * Flushes immutable MemTable to Level 0 SSTable                              |
|  * Compactor thread merges overlapping L0 SSTables into sorted L1 files        |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| DISK SSTABLE SUBSYSTEM (4KB Block Alignment)                                  |
|                                                                               |
|  [Data Blocks] -> [Sparse Block Index] -> [Bloom Filter] -> [SSTable Footer]  |
+-------------------------------------------------------------------------------+
```

---

## 🔬 Key Engineering Highlights

1. **Zero Random Write Amplification**: All writes hit an in-memory SkipList and an append-only WAL.
2. **$O(1)$ Negative Lookup Rejection**: Integrated Bloom filter eliminates $>99\%$ of unnecessary disk reads for absent keys.
3. **Sparse Binary-Indexed SSTables**: 4KB data blocks are indexed by their maximum key, enabling rapid single-block seek retrieval.
4. **Background Compaction Worker**: Asynchronous background thread compacts multiple small SSTables into contiguous sorted runs to maintain bounded read amplification.
