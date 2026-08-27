#pragma once

#include <cstdint>
#include <cstddef>
#include <ucontext.h>
#include <sys/mman.h>
#include <functional>
#include <deque>
#include <memory>
#include <vector>
#include <cassert>
#include <iostream>

namespace sys::fiber {

enum class FiberState : uint8_t {
    READY,
    RUNNING,
    SUSPENDED,
    DEAD
};

class Scheduler;

class Fiber : public std::enable_shared_from_this<Fiber> {
public:
    static constexpr size_t STACK_SIZE = 64 * 1024; // 64 KB

    explicit Fiber(uint64_t id, std::function<void()> fn)
        : id_(id), fn_(std::move(fn)) {
        stack_mem_ = ::mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (stack_mem_ == MAP_FAILED) {
            throw std::bad_alloc();
        }

        ::getcontext(&ctx_);
        ctx_.uc_stack.ss_sp = stack_mem_;
        ctx_.uc_stack.ss_size = STACK_SIZE;
        ctx_.uc_link = nullptr;
        ::makecontext(&ctx_, reinterpret_cast<void(*)()>(fiber_entry), 1, this);
    }

    ~Fiber() {
        if (stack_mem_ && stack_mem_ != MAP_FAILED) {
            ::munmap(stack_mem_, STACK_SIZE);
        }
    }

    Fiber(const Fiber&) = delete;
    Fiber& operator=(const Fiber&) = delete;

    [[nodiscard]] uint64_t id() const noexcept { return id_; }
    [[nodiscard]] FiberState state() const noexcept { return state_; }
    void set_state(FiberState s) noexcept { state_ = s; }
    [[nodiscard]] ucontext_t* context() noexcept { return &ctx_; }

private:
    static void fiber_entry(Fiber* self) {
        if (self && self->fn_) {
            self->fn_();
        }
        self->state_ = FiberState::DEAD;
        // Yield back to scheduler
        yield();
    }

    static void yield();

    uint64_t id_{0};
    void* stack_mem_{nullptr};
    ucontext_t ctx_{};
    FiberState state_{FiberState::READY};
    std::function<void()> fn_;

    friend class Scheduler;
};

class Scheduler {
public:
    Scheduler() {
        current_scheduler_ = this;
    }

    ~Scheduler() {
        if (current_scheduler_ == this) {
            current_scheduler_ = nullptr;
        }
    }

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    template <typename Func>
    uint64_t spawn(Func&& fn) {
        const uint64_t id = ++next_id_;
        auto fiber = std::make_shared<Fiber>(id, std::forward<Func>(fn));
        ready_queue_.push_back(fiber);
        return id;
    }

    static void yield() {
        if (!current_scheduler_ || !current_fiber_) return;

        Fiber* f = current_fiber_;
        if (f->state() == FiberState::RUNNING) {
            f->set_state(FiberState::READY);
            current_scheduler_->ready_queue_.push_back(current_fiber_shared_);
        }

        ::swapcontext(f->context(), &current_scheduler_->sched_ctx_);
    }

    void run() {
        while (!ready_queue_.empty()) {
            auto fiber = ready_queue_.front();
            ready_queue_.pop_front();

            if (fiber->state() == FiberState::DEAD) continue;

            fiber->set_state(FiberState::RUNNING);
            current_fiber_ = fiber.get();
            current_fiber_shared_ = fiber;

            ::swapcontext(&sched_ctx_, fiber->context());

            current_fiber_ = nullptr;
            current_fiber_shared_ = nullptr;
        }
    }

    static Scheduler* current() noexcept { return current_scheduler_; }
    static Fiber* current_fiber() noexcept { return current_fiber_; }
    static std::shared_ptr<Fiber> current_fiber_shared() noexcept { return current_fiber_shared_; }

    void resume(std::shared_ptr<Fiber> fiber) {
        if (fiber && fiber->state() != FiberState::DEAD) {
            fiber->set_state(FiberState::READY);
            ready_queue_.push_back(std::move(fiber));
        }
    }

private:
    uint64_t next_id_{0};
    ucontext_t sched_ctx_{};
    std::deque<std::shared_ptr<Fiber>> ready_queue_;

    inline static thread_local Scheduler* current_scheduler_{nullptr};
    inline static thread_local Fiber* current_fiber_{nullptr};
    inline static thread_local std::shared_ptr<Fiber> current_fiber_shared_{nullptr};
};

inline void Fiber::yield() {
    Scheduler::yield();
}

/**
 * @brief Fiber-Aware Non-Blocking Cooperative Mutex.
 */
class FiberMutex {
public:
    void lock() {
        while (locked_) {
            auto current_fib = Scheduler::current_fiber_shared();
            if (current_fib) {
                current_fib->set_state(FiberState::SUSPENDED);
                waiters_.push_back(current_fib);
                Scheduler::yield();
            }
        }
        locked_ = true;
    }

    void unlock() {
        locked_ = false;
        if (!waiters_.empty()) {
            auto next_fib = waiters_.front();
            waiters_.pop_front();
            if (Scheduler::current()) {
                Scheduler::current()->resume(next_fib);
            }
        }
    }

private:
    bool locked_{false};
    std::deque<std::shared_ptr<Fiber>> waiters_;
};

/**
 * @brief Fiber-Aware Cooperative Channel for inter-fiber communication.
 */
template <typename T>
class FiberChannel {
public:
    explicit FiberChannel(size_t capacity = 16) : capacity_(capacity) {}

    void send(const T& val) {
        while (queue_.size() >= capacity_) {
            auto current_fib = Scheduler::current_fiber_shared();
            if (current_fib) {
                current_fib->set_state(FiberState::SUSPENDED);
                send_waiters_.push_back(current_fib);
                Scheduler::yield();
            }
        }

        queue_.push_back(val);

        if (!recv_waiters_.empty()) {
            auto next_fib = recv_waiters_.front();
            recv_waiters_.pop_front();
            if (Scheduler::current()) Scheduler::current()->resume(next_fib);
        }
    }

    bool receive(T& out) {
        while (queue_.empty()) {
            auto current_fib = Scheduler::current_fiber_shared();
            if (current_fib) {
                current_fib->set_state(FiberState::SUSPENDED);
                recv_waiters_.push_back(current_fib);
                Scheduler::yield();
            } else {
                return false;
            }
        }

        out = std::move(queue_.front());
        queue_.pop_front();

        if (!send_waiters_.empty()) {
            auto next_fib = send_waiters_.front();
            send_waiters_.pop_front();
            if (Scheduler::current()) Scheduler::current()->resume(next_fib);
        }
        return true;
    }

private:
    size_t capacity_;
    std::deque<T> queue_;
    std::deque<std::shared_ptr<Fiber>> send_waiters_;
    std::deque<std::shared_ptr<Fiber>> recv_waiters_;
};

} // namespace sys::fiber
