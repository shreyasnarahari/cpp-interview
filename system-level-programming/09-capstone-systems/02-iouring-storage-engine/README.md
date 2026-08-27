# Capstone 02: High-Throughput `io_uring` WAL & Storage Engine

A production-grade, crash-resilient Key-Value storage engine featuring an Append-Only Write-Ahead Log (WAL), 4KB block sector alignment, asynchronous batch submission via Linux `io_uring`, in-memory indexing, and automated recovery.

---

## 🏛️ System Architecture

```
+-------------------------------------------------------------------------------+
| CLIENT APPLICATION (Put / Get / Delete)                                       |
+---------------------------------------|---------------------------------------+
                                        v
+-------------------------------------------------------------------------------+
| WAL STORAGE ENGINE (In-Memory Layer)                                          |
|                                                                               |
|  1. Fast Path Read: In-Memory Index (std::unordered_map / Concurrent Hash)   |
|  2. Fast Path Write: Formats 4KB-Aligned WalRecord                            |
|  3. Buffers write into SQ ring batch                                          |
+---------------------------------------|---------------------------------------+
                                        v (io_uring Batch Commit)
+-------------------------------------------------------------------------------+
| LINUX io_uring ASYNC I/O ENGINE                                               |
|                                                                               |
|  * Submission Queue (SQ) batching of Log Records                              |
|  * O_DIRECT / 4KB aligned physical writes                                     |
|  * Asynchronous completion polling via Completion Queue (CQ)                  |
+---------------------------------------|---------------------------------------+
                                        v (write to disk)
+-------------------------------------------------------------------------------+
| PERSISTENT DISK (Append-Only WAL File)                                        |
|                                                                               |
|  [Record 0 (4KB)] [Record 1 (4KB)] [Record 2 (4KB)] ... [Record N (4KB)]      |
+-------------------------------------------------------------------------------+
```

---

## 🔬 Crash Recovery Mechanics

Upon restarting after an unexpected system shutdown:
1. The engine opens the existing WAL file.
2. Linearly reads 4KB records from offset 0 to EOF.
3. Validates record checksums and sequence numbers.
4. Reconstructs the in-memory index by replaying each valid transaction.
5. Truncates any partial/corrupted trailing writes.
