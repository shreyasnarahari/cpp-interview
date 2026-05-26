# Design Patterns — Easy Questions

---

**Q1. What is the Singleton pattern?**

A: Ensures a class has **exactly one instance** with a global access point. In modern C++, use Meyer's Singleton:

```cpp
class Logger {
public:
    static Logger& instance() {
        static Logger inst;  // thread-safe since C++11
        return inst;
    }

    void log(const std::string& msg) {
        std::lock_guard lock(mtx_);
        std::cout << "[LOG] " << msg << "\n";
    }

    // Delete copy/move
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;
    std::mutex mtx_;
};

// Usage:
Logger::instance().log("Hello");
```

**When NOT to use:** when you need testability (dependency injection is better), when global state causes coupling. Singleton is the most overused pattern.

---

**Q2. What is the Factory Method pattern?**

A: A method that creates objects without specifying the exact class — delegates creation to subclasses or a registry:

```cpp
// Product hierarchy
class Shape {
public:
    virtual void draw() const = 0;
    virtual ~Shape() = default;
};

class Circle : public Shape {
public:
    void draw() const override { std::cout << "Drawing circle\n"; }
};

class Square : public Shape {
public:
    void draw() const override { std::cout << "Drawing square\n"; }
};

// Factory
class ShapeFactory {
public:
    static std::unique_ptr<Shape> create(const std::string& type) {
        if (type == "circle") return std::make_unique<Circle>();
        if (type == "square") return std::make_unique<Square>();
        throw std::invalid_argument("Unknown shape: " + type);
    }
};

auto shape = ShapeFactory::create("circle");
shape->draw();
```

---

**Q3. What is the Strategy pattern?**

A: Encapsulate interchangeable algorithms behind a common interface:

```cpp
// Runtime strategy (virtual functions)
class Compressor {
public:
    virtual std::vector<uint8_t> compress(const std::vector<uint8_t>& data) = 0;
    virtual ~Compressor() = default;
};

class GzipCompressor : public Compressor {
public:
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) override {
        // gzip implementation
        return data;
    }
};

class FileWriter {
    std::unique_ptr<Compressor> compressor_;
public:
    void setCompressor(std::unique_ptr<Compressor> c) {
        compressor_ = std::move(c);
    }
    void write(const std::vector<uint8_t>& data) {
        auto compressed = compressor_->compress(data);
        // write compressed data
    }
};
```

---

**Q4. What is the Observer pattern?**

A: One-to-many dependency where subjects notify observers of state changes:

```cpp
class IObserver {
public:
    virtual void onUpdate(const std::string& event, int data) = 0;
    virtual ~IObserver() = default;
};

class EventBus {
    std::unordered_map<std::string, std::vector<std::weak_ptr<IObserver>>> listeners_;
public:
    void subscribe(const std::string& event, std::shared_ptr<IObserver> obs) {
        listeners_[event].push_back(obs);
    }

    void publish(const std::string& event, int data) {
        auto& observers = listeners_[event];
        // Remove expired weak_ptrs and notify alive ones
        observers.erase(
            std::remove_if(observers.begin(), observers.end(),
                [](auto& wp) { return wp.expired(); }),
            observers.end());

        for (auto& wp : observers) {
            if (auto sp = wp.lock()) sp->onUpdate(event, data);
        }
    }
};
```

Using `weak_ptr` prevents the observer from keeping the subject alive and avoids dangling pointers when observers are destroyed.

---

## Coding Questions

---

**C1. Self-registering factory**

A:
```cpp
class Shape {
public:
    virtual void draw() const = 0;
    virtual ~Shape() = default;

    // Registry
    using Creator = std::function<std::unique_ptr<Shape>()>;
    static std::unordered_map<std::string, Creator>& registry() {
        static std::unordered_map<std::string, Creator> reg;
        return reg;
    }

    static std::unique_ptr<Shape> create(const std::string& name) {
        auto it = registry().find(name);
        if (it == registry().end()) return nullptr;
        return it->second();
    }
};

// Macro for self-registration
#define REGISTER_SHAPE(Type) \
    static bool _reg_##Type = []() { \
        Shape::registry()[#Type] = []() { return std::make_unique<Type>(); }; \
        return true; \
    }()

class Triangle : public Shape {
public:
    void draw() const override { std::cout << "Triangle\n"; }
};
REGISTER_SHAPE(Triangle);

// Usage:
auto shape = Shape::create("Triangle");
shape->draw();
```

---

**C2. Compile-time Strategy (template-based)**

A:
```cpp
// Policy classes
struct QuickSortPolicy {
    template <typename It>
    static void sort(It begin, It end) { std::sort(begin, end); }
};

struct StableSortPolicy {
    template <typename It>
    static void sort(It begin, It end) { std::stable_sort(begin, end); }
};

// Host class parameterized by policy
template <typename SortPolicy = QuickSortPolicy>
class DataProcessor {
    std::vector<int> data_;
public:
    void addData(int val) { data_.push_back(val); }
    void process() {
        SortPolicy::sort(data_.begin(), data_.end());
    }
};

DataProcessor<StableSortPolicy> processor;  // uses stable sort
```
