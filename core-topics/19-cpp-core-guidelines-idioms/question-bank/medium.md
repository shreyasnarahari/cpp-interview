# Intermediate Questions: The Pimpl Pattern

### Q1: Write a complete Pimpl class implementation.
```cpp
// Header: Widget.h
#include <memory>
class Widget {
public:
    Widget();
    ~Widget();
    Widget(Widget&&) noexcept;
    Widget& operator=(Widget&&) noexcept;
    void render();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Source: Widget.cpp
#include "Widget.h"
#include <iostream>

struct Widget::Impl {
    void render() { std::cout << "Rendering Widget\n"; }
};

Widget::Widget() : impl_(std::make_unique<Impl>()) {}
Widget::~Widget() = default;
Widget::Widget(Widget&&) noexcept = default;
Widget& Widget::operator=(Widget&&) noexcept = default;
void Widget::render() { impl_->render(); }
```
