# Exception Handling & noexcept — Easy Questions

---

**Q1. How do you throw and catch an exception?**

A:
```cpp
void validateAge(int age) {
    if (age < 0) throw std::invalid_argument("Age cannot be negative");
    if (age > 150) throw std::out_of_range("Age too large");
}

int main() {
    try {
        validateAge(-5);
    } catch (const std::invalid_argument& e) {
        std::cerr << "Invalid: " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "Unknown error\n";
    }
}
```

**Rules:**
- Always catch by **const reference** (`const std::exception&`)
- Catch more specific types first (derived before base)
- Use `catch (...)` as a last resort

---

**Q2. What is stack unwinding?**

A: When an exception is thrown, the runtime **destroys all local objects** in reverse order as it unwinds the call stack, searching for a matching `catch` handler:

```cpp
void inner() {
    std::string s = "hello";    // (3) destroyed during unwinding
    throw std::runtime_error("error");
}

void middle() {
    std::vector<int> v = {1,2,3}; // (2) destroyed during unwinding
    inner();
}

void outer() {
    try {
        std::unique_ptr<int> p = std::make_unique<int>(42);  // (1) destroyed
        middle();
    } catch (const std::exception& e) {
        // caught here — all locals in inner, middle, outer are destroyed
        std::cout << e.what();
    }
}
```

This is why RAII works — destructors run even during exceptions.

---

**Q3. What is `noexcept`?**

A: A specifier that promises a function will never throw an exception:

```cpp
void safe() noexcept {
    // guaranteed not to throw
}

void risky() {
    // might throw
}
```

**If a `noexcept` function DOES throw** → `std::terminate()` is called. The program crashes immediately (no stack unwinding).

Why use it:
1. **Enables optimizations** — compiler doesn't generate unwinding code
2. **Critical for move operations** — `std::vector` only moves if move ctor is `noexcept`
3. **Documents intent** — promises to callers

---

**Q4. Why catch by const reference?**

A: Three reasons:
1. **Avoids slicing**: catching `Base` by value slices off derived information
2. **Avoids copy**: references don't copy the exception object
3. **Const correctness**: you rarely need to modify the exception

```cpp
// WRONG — slices derived exception:
catch (std::exception e) { /* loses derived info */ }

// WRONG — unnecessary non-const:
catch (std::exception& e) { /* works but why mutable? */ }

// CORRECT:
catch (const std::exception& e) { std::cerr << e.what(); }
```

---

**Q5. What is `throw;` vs `throw e;`?**

A:
```cpp
try {
    throw DerivedError("oops");
} catch (const BaseError& e) {
    throw;    // re-throws the ORIGINAL DerivedError (preserves type)
    // vs
    throw e;  // throws a COPY of e as BaseError (SLICES the derived part!)
}
```

**Always use `throw;`** to re-throw. `throw e;` creates a sliced copy.

---

## Coding Questions

---

**C1. Fix the resource leak**

```cpp
void process() {
    int* data = new int[1000];
    riskyOperation();    // might throw!
    delete[] data;       // skipped if exception
}
```

A:
```cpp
void process() {
    auto data = std::make_unique<int[]>(1000);
    riskyOperation();    // if this throws, data is still freed by RAII
}
```

---

**C2. Custom exception class**

A:
```cpp
class AppError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;  // inherit constructor
};

class NetworkError : public AppError {
    int statusCode_;
public:
    NetworkError(const std::string& msg, int code)
        : AppError(msg), statusCode_(code) {}
    int statusCode() const { return statusCode_; }
};

class DatabaseError : public AppError {
    std::string query_;
public:
    DatabaseError(const std::string& msg, std::string query)
        : AppError(msg), query_(std::move(query)) {}
    const std::string& query() const { return query_; }
};

// Usage:
try {
    throw NetworkError("Connection refused", 503);
} catch (const NetworkError& e) {
    std::cerr << e.what() << " (HTTP " << e.statusCode() << ")\n";
} catch (const AppError& e) {
    std::cerr << "App error: " << e.what() << "\n";
}
```

---

**C3. RAII cleanup on exception**

A:
```cpp
class Transaction {
    Database& db_;
    bool committed_ = false;
public:
    Transaction(Database& db) : db_(db) { db_.begin(); }

    ~Transaction() {
        if (!committed_) db_.rollback();  // auto-rollback on exception
    }

    void commit() {
        db_.commit();
        committed_ = true;
    }
};

void transferMoney(Database& db, int from, int to, int amount) {
    Transaction txn(db);
    db.debit(from, amount);    // might throw
    db.credit(to, amount);     // might throw
    txn.commit();              // only reached if both succeed
    // if exception → ~Transaction rolls back
}
```

---

**C4. noexcept conditional**

A:
```cpp
// noexcept can be conditional:
template <typename T>
void swap(T& a, T& b) noexcept(noexcept(T(std::move(a)))) {
    T tmp = std::move(a);
    a = std::move(b);
    b = std::move(tmp);
}
// This swap is noexcept only if T's move constructor is noexcept.
// noexcept(expr) as operator → returns true if expr is noexcept
// noexcept(bool) as specifier → marks function as noexcept if bool is true
```
