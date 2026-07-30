# Advanced Questions: Zero-Overhead Error Handling & Assembly

### Q1: Implement a custom lightweight `Result<T, E>` monad for pre-C++23 environments.
```cpp
template <typename T, typename E>
class Result {
    union {
        T value_;
        E error_;
    };
    bool is_ok_;
public:
    Result(T val) : value_(std::move(val)), is_ok_(true) {}
    Result(E err) : error_(std::move(err)), is_ok_(false) {}
    ~Result() {
        if (is_ok_) value_.~T();
        else error_.~E();
    }
    bool is_ok() const { return is_ok_; }
    const T& value() const { return value_; }
    const E& error() const { return error_; }
};
```
