# Module 09: Transaction Processing, MVCC, and Recovery

An exhaustive, CMU 15-445 / 15-721 standard database kernel guide to ACID guarantees, ANSI isolation levels, concurrency control (Strict 2PL vs MVCC), Multi-Version Concurrency Control internals (Undo Logs, Read Views, Version Chains), and the ARIES Crash Recovery Algorithm.

---

## 1. ACID Guarantees & Transaction Isolation Levels

| Property | Guarantee | Database Kernel Implementation |
|---|---|---|
| **Atomicity** | All-or-Nothing execution of transaction statements. | Write-Ahead Logging (WAL) + Undo Log rollback on abort. |
| **Consistency** | Database transitions only between valid states satisfying constraints. | Application & transaction logic over atomic operations. |
| **Isolation** | Concurrent transactions execute without mutual interference. | Concurrency Control (2PL, Timestamp Ordering, MVCC). |
| **Durability** | Committed state survives crashes and power outages. | Flushing WAL logs to non-volatile disk (`fsync`). |

### Concurrency Anomalies & ANSI Isolation Levels:
- **Dirty Read ($G_1$)**: Reading uncommitted modifications of another transaction.
- **Non-Repeatable Read ($G_{2a}$)**: Re-reading the same row returns different values because another transaction committed an update in between.
- **Phantom Read ($A3$)**: Re-running a range query returns newly inserted rows.

| Isolation Level | Dirty Reads Allowed? | Non-Repeatable Reads Allowed? | Phantom Reads Allowed? |
|---|---|---|---|
| **Read Uncommitted** | Yes | Yes | Yes |
| **Read Committed** | **No** | Yes | Yes |
| **Repeatable Read (Snapshot Isolation)** | **No** | **No** | Yes (Prevented in MVCC) |
| **Serializable** | **No** | **No** | **No** |

---

## 2. Multi-Version Concurrency Control (MVCC) Architecture

MVCC avoids reader-writer blocking: **"Readers never block Writers, and Writers never block Readers."**

```
+-------------------------------------------------------------------------------+
| TABLE ROW HEAP (Current Visible Tuple Version)                                |
|                                                                               |
|  Tuple ID: 101 | Key: AAPL | Price: $195.00 | xmin: Txn 300 | roll_ptr: 0x8000|
+-------------------------------------------------------------------|-----------+
                                                                    v
+-------------------------------------------------------------------------------+
| UNDO LOG STORE (Historical Rollback Segments)                                 |
|                                                                               |
|  [Undo Record 1 (0x8000)]                                                     |
|  - modified_by: Txn 200                                                       |
|  - old_values:  Price = $190.00                                               |
|  - prev_ptr:    0x7000 ----------------------------+                          |
|                                                    v                          |
|  [Undo Record 2 (0x7000)]                                                     |
|  - modified_by: Txn 100                                                       |
|  - old_values:  Price = $180.00                                               |
|  - prev_ptr:    nullptr                                                       |
+-------------------------------------------------------------------------------+
```

### The `ReadView` Snapshot Visibility Algorithm:
When Transaction $T$ begins with a `ReadView`:
1. `m_low_limit_id`: Highest committed Txn ID + 1 when view was created.
2. `m_up_limit_id`: Lowest active (uncommitted) Txn ID.
3. `m_ids`: Set of active Txn IDs when view was created.

**Visibility Rules**:
- If `tuple.xmin < m_up_limit_id` $\implies$ **Visible** (committed before snapshot started).
- If `tuple.xmin >= m_low_limit_id` $\implies$ **Invisible** (created after snapshot started; walk `roll_ptr` to older undo log).
- If `tuple.xmin \in m_ids` $\implies$ **Invisible** (was active/uncommitted when snapshot started; walk `roll_ptr`).
- If `tuple.xmin == current_txn_id` $\implies$ **Visible** (own transaction modifications).

---

## 3. Crash Recovery: The ARIES Algorithm

ARIES (Algorithms for Recovery and Isolation Exploiting Semantics) restores a consistent database state after unexpected system failure:

```
+-------------------------------------------------------------------------------+
| ARIES RECOVERY PHASES (On Restart)                                            |
|                                                                               |
|  1. ANALYSIS PHASE:                                                           |
|     - Scans WAL forward from last checkpoint.                                 |
|     - Reconstructs Dirty Page Table (DPT) and Active Transaction Table (ATT). |
|                                                                               |
|  2. REDO PHASE (Repeating History):                                           |
|     - Scans WAL forward from smallest recLSN in DPT to the end of the log.    |
|     - Reapplies all logged changes (for BOTH committed and uncommitted txns!) |
|       if page.LSN < log.LSN.                                                  |
|                                                                               |
|  3. UNDO PHASE (Rolling Back Losers):                                         |
|     - Scans WAL backward from log end.                                        |
|     - Reverses actions of all uncommitted transactions active at crash time.  |
|     - Writes Compensation Log Records (CLRs) to prevent cascading aborts.     |
+-------------------------------------------------------------------------------+
```

---

## 💻 Lab Exercises & Projects in this Module

1. **`exercises/mvcc_undo_log.hpp` & `exercises/mvcc_undo_log_traversal.cpp`**:
   - Production MVCC version chain walker traversing Undo Log records to reconstruct consistent point-in-time snapshots based on active Read Views.
2. **`tests/test_mvcc.cpp`**:
   - Unit tests validating Snapshot Isolation, preventing dirty and non-repeatable reads.
3. **`projects/mini_transactional_engine/`**:
   - Lightweight ACID transactional storage engine supporting `BEGIN`, `COMMIT`, `ABORT`, snapshot queries, and ARIES-style WAL crash recovery replay.
