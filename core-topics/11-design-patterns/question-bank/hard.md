# Design Patterns — Hard Questions

---

**Q1. What is type erasure?**

A: Hiding concrete types behind a uniform interface — virtual functions internally, value semantics externally. `std::function`, `std::any`, and `std::packaged_task` all use this.

```cpp
class AnyDrawable {
    struct Concept {
        virtual void draw() const = 0;
        virtual std::unique_ptr<Concept> clone() const = 0;
        virtual ~Concept() = default;
    };
    template <typename T>
    struct Model : Concept {
        T obj;
        Model(T o) : obj(std::move(o)) {}
        void draw() const override { obj.draw(); }
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(obj);
        }
    };
    std::unique_ptr<Concept> impl_;
public:
    template <typename T>
    AnyDrawable(T obj) : impl_(std::make_unique<Model<T>>(std::move(obj))) {}
    AnyDrawable(const AnyDrawable& o) : impl_(o.impl_->clone()) {}
    AnyDrawable(AnyDrawable&&) noexcept = default;
    AnyDrawable& operator=(AnyDrawable o) { impl_ = std::move(o.impl_); return *this; }
    void draw() const { impl_->draw(); }
};

// ANY type with .draw() works — no inheritance required:
struct Circle { void draw() const { std::cout << "O"; } };
struct Square { void draw() const { std::cout << "[]"; } };

std::vector<AnyDrawable> shapes;
shapes.push_back(Circle{});
shapes.push_back(Square{});
for (auto& s : shapes) s.draw();
```

Key insight: **templates at the boundary, virtuals inside, value semantics outside**.

---

**Q2. What is policy-based design?**

A: Using template parameters to inject behavior at compile time — compile-time Strategy pattern:

```cpp
// Policies
struct NullLock {
    void lock() {}
    void unlock() {}
};
struct MutexLock {
    std::mutex mtx_;
    void lock() { mtx_.lock(); }
    void unlock() { mtx_.unlock(); }
};

struct NoLogging {
    void log(const std::string&) {}
};
struct ConsoleLogging {
    void log(const std::string& msg) { std::cout << "[LOG] " << msg << "\n"; }
};

// Host class composed from policies
template <typename LockPolicy = NullLock, typename LogPolicy = NoLogging>
class Cache : private LockPolicy, private LogPolicy {
    std::unordered_map<std::string, int> data_;
public:
    void put(const std::string& key, int val) {
        this->lock();
        this->log("put: " + key);
        data_[key] = val;
        this->unlock();
    }
    int get(const std::string& key) {
        this->lock();
        this->log("get: " + key);
        int v = data_[key];
        this->unlock();
        return v;
    }
};

Cache<NullLock, NoLogging> fastCache;            // zero overhead
Cache<MutexLock, ConsoleLogging> debugCache;      // thread-safe + logged
```

Advantages over runtime polymorphism: zero overhead (no vtable), full inlining, composable.

---

**Q3. How does `std::variant` + `std::visit` replace the Visitor pattern?**

A: C++17 `std::variant` provides type-safe unions with pattern matching:

```cpp
// Old way: class hierarchy + visitor
// New way: variant + visit
using Shape = std::variant<Circle, Rectangle, Triangle>;

double area(const Shape& s) {
    return std::visit([](const auto& shape) -> double {
        if constexpr (std::is_same_v<std::decay_t<decltype(shape)>, Circle>)
            return M_PI * shape.radius * shape.radius;
        else if constexpr (std::is_same_v<std::decay_t<decltype(shape)>, Rectangle>)
            return shape.w * shape.h;
        else
            return 0.5 * shape.base * shape.height;
    }, s);
}

// Or with overloaded lambda pattern:
template <typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template <typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;

double area2(const Shape& s) {
    return std::visit(overloaded{
        [](const Circle& c) { return M_PI * c.radius * c.radius; },
        [](const Rectangle& r) { return r.w * r.h; },
        [](const Triangle& t) { return 0.5 * t.base * t.height; },
    }, s);
}
```

Advantages: no virtual functions, no heap allocation, closed set of types checked at compile time.

---

## Coding Questions

---

**C1. Simplified `std::any`**

A:
```cpp
class Any {
    struct Concept {
        virtual ~Concept() = default;
        virtual std::unique_ptr<Concept> clone() const = 0;
        virtual const std::type_info& type() const = 0;
    };
    template <typename T>
    struct Model : Concept {
        T value;
        Model(T v) : value(std::move(v)) {}
        std::unique_ptr<Concept> clone() const override {
            return std::make_unique<Model>(value);
        }
        const std::type_info& type() const override { return typeid(T); }
    };
    std::unique_ptr<Concept> impl_;
public:
    Any() = default;
    template <typename T>
    Any(T value) : impl_(std::make_unique<Model<std::decay_t<T>>>(std::move(value))) {}
    Any(const Any& o) : impl_(o.impl_ ? o.impl_->clone() : nullptr) {}
    Any(Any&&) noexcept = default;
    Any& operator=(Any o) { impl_ = std::move(o.impl_); return *this; }

    bool has_value() const { return impl_ != nullptr; }
    const std::type_info& type() const { return impl_ ? impl_->type() : typeid(void); }

    template <typename T>
    T& get() {
        if (type() != typeid(T)) throw std::bad_cast();
        return static_cast<Model<T>*>(impl_.get())->value;
    }
};

Any a = 42;
Any b = std::string("hello");
std::cout << a.get<int>();           // 42
std::cout << b.get<std::string>();   // hello
```

---

**C2. Plugin factory with self-registration**

A:
```cpp
class Plugin {
public:
    virtual std::string name() const = 0;
    virtual void execute() = 0;
    virtual ~Plugin() = default;
};

class PluginRegistry {
    std::unordered_map<std::string, std::function<std::unique_ptr<Plugin>()>> creators_;
    PluginRegistry() = default;
public:
    static PluginRegistry& instance() {
        static PluginRegistry reg;
        return reg;
    }
    void reg(const std::string& name, std::function<std::unique_ptr<Plugin>()> fn) {
        creators_[name] = std::move(fn);
    }
    std::unique_ptr<Plugin> create(const std::string& name) {
        auto it = creators_.find(name);
        return it != creators_.end() ? it->second() : nullptr;
    }
    std::vector<std::string> list() const {
        std::vector<std::string> names;
        for (auto& [k, _] : creators_) names.push_back(k);
        return names;
    }
};

// Auto-registration helper
template <typename T>
struct AutoRegister {
    AutoRegister() {
        PluginRegistry::instance().reg(T::pluginName(),
            []() { return std::make_unique<T>(); });
    }
};

class JsonPlugin : public Plugin {
    static AutoRegister<JsonPlugin> reg_;
public:
    static const char* pluginName() { return "json"; }
    std::string name() const override { return "json"; }
    void execute() override { std::cout << "Processing JSON\n"; }
};
AutoRegister<JsonPlugin> JsonPlugin::reg_;

// Usage:
auto p = PluginRegistry::instance().create("json");
p->execute();
```

---

**C3. Overloaded lambda pattern (C++17)**

A:
```cpp
template <typename... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

using Event = std::variant<MouseClick, KeyPress, WindowResize>;

void handleEvent(const Event& e) {
    std::visit(overloaded{
        [](const MouseClick& m) { std::cout << "Click at " << m.x << "," << m.y; },
        [](const KeyPress& k) { std::cout << "Key: " << k.key; },
        [](const WindowResize& w) { std::cout << "Resize: " << w.w << "x" << w.h; },
    }, e);
}
```

This replaces the Visitor pattern with zero overhead and compile-time exhaustiveness checking.
