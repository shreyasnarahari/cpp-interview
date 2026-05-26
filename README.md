# C++ Interview Prep

A structured, 80/20 study system for C++ engineering interviews.

---

## What's Here

```
cpp-interview/
├── syllabus.txt          ← Full 20-topic C++ syllabus (the complete map)
├── real-interview-q/     ← Curated questions actually asked at real companies
├── core-topics/          ← Deep-dive study material for the 9 highest-impact topics
└── practice-cpp/         ← Scratch pad for running quick C++ experiments
```

---

## Folders

### `syllabus.txt`
The full C++ language syllabus — 20 topics covering everything from fundamentals to modern C++23 features. Use it as a reference map, not a study checklist. The `core-topics/` folder already picks out the 80/20 subset worth studying.

### `real-interview-q/`
`interview-questions.md` — Questions organized by topic and tagged by company (Google, Meta, Microsoft, Amazon, Apple, Bloomberg, Jane Street). Includes gotcha code snippets with answers. Good for a high-level sweep before an interview or to identify gaps.

### `core-topics/`
The main study folder. 9 topics that cover ~80% of what gets asked in real C++ interviews. Each topic has structured question banks (easy → medium → hard), coding problems with answers, and a reading list.

See [core-topics/README.md](./core-topics/README.md) for the full topic list and study approach.

### `practice-cpp/`
Minimal CMake project for running quick experiments. Edit `main.cpp`, build, and run.

---

## Suggested Workflow

1. **Scan** `real-interview-q/interview-questions.md` to get a feel for the question landscape
2. **Study** topics from `core-topics/` in order (01 → 09), starting with P1 topics
3. **Drill** `question-bank/real-interview-q-a.md` for each topic cold — no peeking
4. **Review** `syllabus.txt` at the end to spot any remaining gaps
