# Advanced Questions: Type Erasure Implementation

### Q1: Implement a custom `AnyFunction<R(Args...)>` type-erasure container.
```cpp
#include <memory>
#include <utility>

template <typename Signature>
class AnyFunction;

template <typename R, typename... Args>
class AnyFunction<R(Args...)> {
    struct Concept {
        virtual ~Concept() = default;
        virtual R invoke(Args... args) = 0;
    };

    template <typename F>
    struct Model : Concept {
        F fn_;
        Model(F fn) : fn_(std::move(fn)) {}
        R invoke(Args... args) override { return fn_(std::forward<Args>(args)...); }
    };

    std::unique_ptr<Concept> pimpl_;
public:
    template <typename F>
    AnyFunction(F fn) : pimpl_(std::make_unique<Model<F>>(std::move(fn))) {}

    R operator()(Args... args) const {
        return pimpl_->invoke(std::forward<Args>(args)...);
    }
};
```
