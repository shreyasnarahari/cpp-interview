#include "fiber_scheduler.hpp"
#include <iostream>
#include <vector>
#include <cassert>

void test_basic_fiber_execution() {
    std::cout << "[Test 1] Testing Basic Cooperative Fiber Execution & Yielding ...\n";
    sys::fiber::Scheduler sched;

    std::vector<int> execution_order;

    sched.spawn([&] {
        execution_order.push_back(1);
        sys::fiber::Scheduler::yield();
        execution_order.push_back(3);
    });

    sched.spawn([&] {
        execution_order.push_back(2);
        sys::fiber::Scheduler::yield();
        execution_order.push_back(4);
    });

    sched.run();

    assert(execution_order.size() == 4);
    assert(execution_order[0] == 1);
    assert(execution_order[1] == 2);
    assert(execution_order[2] == 3);
    assert(execution_order[3] == 4);

    std::cout << "  Cooperative interleaving verified: [1, 2, 3, 4].\n";
}

void test_fiber_mutex() {
    std::cout << "[Test 2] Testing FiberMutex Critical Section Synchronization ...\n";
    sys::fiber::Scheduler sched;
    sys::fiber::FiberMutex mtx;
    int shared_val = 0;

    for (int i = 0; i < 50; ++i) {
        sched.spawn([&] {
            mtx.lock();
            int cur = shared_val;
            sys::fiber::Scheduler::yield();
            shared_val = cur + 1;
            mtx.unlock();
        });
    }

    sched.run();
    assert(shared_val == 50);
    std::cout << "  FiberMutex protected shared variable across 50 fibers (final value: 50).\n";
}

void test_fiber_channel() {
    std::cout << "[Test 3] Testing FiberChannel Producer-Consumer Communication ...\n";
    sys::fiber::Scheduler sched;
    sys::fiber::FiberChannel<int> chan(4);
    std::vector<int> received;

    sched.spawn([&] {
        for (int i = 1; i <= 10; ++i) {
            chan.send(i * 10);
        }
    });

    sched.spawn([&] {
        for (int i = 1; i <= 10; ++i) {
            int val = 0;
            if (chan.receive(val)) {
                received.push_back(val);
            }
        }
    });

    sched.run();
    assert(received.size() == 10);
    assert(received[0] == 10);
    assert(received[9] == 100);

    std::cout << "  FiberChannel successfully transferred 10 items through bounded channel.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Capstone 03: Cooperative Fiber Runtime Verification\n";
    std::cout << "=======================================================\n\n";

    test_basic_fiber_execution();
    test_fiber_mutex();
    test_fiber_channel();

    std::cout << "\n>>> ALL FIBER SCHEDULER TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
