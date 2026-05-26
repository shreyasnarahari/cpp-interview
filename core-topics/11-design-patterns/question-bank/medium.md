# Design Patterns — Medium Questions

---

**Q1. What is the Pimpl idiom?**

A: Pointer to Implementation — hides private members behind a forward-declared class. Acts as a **compilation firewall**.

```cpp
// widget.h — PUBLIC HEADER
#include <memory>
class Widget {
public:
    Widget(int x);
    ~Widget();
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;
    void doWork();
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl_;
};

// widget.cpp — heavy headers only here
#include "widget.h"
#include <vector>
#include <string>

struct Widget::Impl {
    int x;
    std::vector<double> cache;
    std::string name;
};

Widget::Widget(int x) : pImpl_(std::make_unique<Impl>()) { pImpl_->x = x; }
Widget::~Widget() = default;  // MUST be in .cpp where Impl is complete
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;
void Widget::doWork() { pImpl_->cache.push_back(pImpl_->x * 1.5); }
```

Benefits: changing `Impl` doesn't recompile users of `widget.h`, ABI stability. Key: destructor MUST be in `.cpp`.

---

**Q2. What is the Builder pattern?**

A: Separates construction of a complex object from its representation:

```cpp
class HttpRequest {
    std::string method_, url_, body_;
    std::unordered_map<std::string, std::string> headers_;
    HttpRequest() = default;
    friend class HttpRequestBuilder;
public:
    const std::string& url() const { return url_; }
};

class HttpRequestBuilder {
    HttpRequest req_;
public:
    HttpRequestBuilder& method(std::string m) { req_.method_ = std::move(m); return *this; }
    HttpRequestBuilder& url(std::string u) { req_.url_ = std::move(u); return *this; }
    HttpRequestBuilder& header(std::string k, std::string v) {
        req_.headers_[std::move(k)] = std::move(v); return *this;
    }
    HttpRequestBuilder& body(std::string b) { req_.body_ = std::move(b); return *this; }
    HttpRequest build() { return std::move(req_); }
};

auto req = HttpRequestBuilder()
    .method("POST")
    .url("https://api.example.com/data")
    .header("Content-Type", "application/json")
    .body(R"({"key": "value"})")
    .build();
```

---

**Q3. What is the Visitor pattern (double dispatch)?**

A: Adds operations to a class hierarchy without modifying the classes:

```cpp
class Circle; class Rectangle;

class ShapeVisitor {
public:
    virtual void visit(Circle& c) = 0;
    virtual void visit(Rectangle& r) = 0;
    virtual ~ShapeVisitor() = default;
};

class Shape {
public:
    virtual void accept(ShapeVisitor& v) = 0;
    virtual ~Shape() = default;
};

class Circle : public Shape {
public:
    double radius;
    Circle(double r) : radius(r) {}
    void accept(ShapeVisitor& v) override { v.visit(*this); }
};

class Rectangle : public Shape {
public:
    double width, height;
    Rectangle(double w, double h) : width(w), height(h) {}
    void accept(ShapeVisitor& v) override { v.visit(*this); }
};

class AreaCalculator : public ShapeVisitor {
public:
    double total = 0;
    void visit(Circle& c) override { total += M_PI * c.radius * c.radius; }
    void visit(Rectangle& r) override { total += r.width * r.height; }
};
```

---

**Q4. What is CRTP used for in practice?**

A: Three main use cases:

```cpp
// 1. Static polymorphism (no vtable overhead)
template <typename Derived>
class Printable {
public:
    void print() const { static_cast<const Derived*>(this)->printImpl(); }
};
class Widget : public Printable<Widget> {
public:
    void printImpl() const { std::cout << "Widget"; }
};

// 2. Instance counting mixin
template <typename Derived>
class Counter {
    static inline int count_ = 0;
public:
    Counter() { count_++; }
    ~Counter() { count_--; }
    static int count() { return count_; }
};
class Widget : public Counter<Widget> {};

// 3. Method chaining returning correct derived type
template <typename Derived>
class Builder {
public:
    Derived& withName(std::string n) { name_ = std::move(n); return static_cast<Derived&>(*this); }
protected:
    std::string name_;
};
```

---

## Coding Questions

---

**C1. Command pattern with undo/redo**

A:
```cpp
class Command {
public:
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual ~Command() = default;
};

class TextEditor {
    std::string text_;
    std::stack<std::unique_ptr<Command>> undoStack_, redoStack_;
public:
    void exec(std::unique_ptr<Command> cmd) {
        cmd->execute();
        undoStack_.push(std::move(cmd));
        while (!redoStack_.empty()) redoStack_.pop();
    }
    void undo() {
        if (undoStack_.empty()) return;
        undoStack_.top()->undo();
        redoStack_.push(std::move(undoStack_.top()));
        undoStack_.pop();
    }
    std::string& text() { return text_; }
};

class InsertText : public Command {
    TextEditor& ed_; size_t pos_; std::string text_;
public:
    InsertText(TextEditor& e, size_t p, std::string t) : ed_(e), pos_(p), text_(std::move(t)) {}
    void execute() override { ed_.text().insert(pos_, text_); }
    void undo() override { ed_.text().erase(pos_, text_.size()); }
};
```

---

**C2. Decorator pattern**

A:
```cpp
class Stream {
public:
    virtual void write(const std::string& data) = 0;
    virtual ~Stream() = default;
};

class FileStream : public Stream {
    std::ofstream file_;
public:
    FileStream(const std::string& path) : file_(path) {}
    void write(const std::string& data) override { file_ << data; }
};

class StreamDecorator : public Stream {
protected:
    std::unique_ptr<Stream> inner_;
public:
    StreamDecorator(std::unique_ptr<Stream> s) : inner_(std::move(s)) {}
};

class BufferedStream : public StreamDecorator {
    std::string buffer_;
public:
    using StreamDecorator::StreamDecorator;
    void write(const std::string& data) override {
        buffer_ += data;
        if (buffer_.size() > 4096) { inner_->write(buffer_); buffer_.clear(); }
    }
};

// Compose: auto s = make_unique<BufferedStream>(make_unique<FileStream>("out.txt"));
```
