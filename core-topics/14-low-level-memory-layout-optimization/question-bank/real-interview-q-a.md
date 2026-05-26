# Low-Level Memory Layout & Optimization — Real Interview Questions

---

**Q1. Optimize the L2 Order Book [Jane Street, Citadel]**

*Interviewer:* "You are designing an order book for a low-latency trading system. We receive millions of updates a second. Below is our `Order` and `Book` structure. How would you optimize the memory layout to improve cache hits when we are constantly calculating the total volume of all bids?"

```cpp
struct Order {
    uint64_t order_id;
    double price;
    uint32_t volume;
    char side; // 'B' or 'A'
    uint64_t timestamp;
};

class OrderBook {
    std::vector<Order> bids;
    std::vector<Order> asks;
    
    uint64_t getTotalBidVolume() {
        uint64_t total = 0;
        for (const auto& order : bids) {
            total += order.volume;
        }
        return total;
    }
};
```

A:
**Step 1: Point out the AoS bottleneck.** 
Currently, `Order` is an Array of Structs (AoS). When `getTotalBidVolume()` iterates through `bids`, it only cares about `volume`. However, the CPU must load the entire `Order` struct into the cache line.
Size of `Order`: `order_id` (8) + `price` (8) + `volume` (4) + `side` (1) + padding (3) + `timestamp` (8) = **32 bytes**.
A 64-byte cache line can only hold **2** orders. We waste 87% of our memory bandwidth loading irrelevant data (`order_id`, `price`, etc.).

**Step 2: Propose SoA (Struct of Arrays).**
Refactor `OrderBook` to separate the fields into contiguous arrays.

```cpp
class OptimizedOrderBook {
    // Parallel arrays
    std::vector<uint64_t> bid_order_ids;
    std::vector<double> bid_prices;
    std::vector<uint32_t> bid_volumes;
    std::vector<uint64_t> bid_timestamps;
    
    uint64_t getTotalBidVolume() {
        uint64_t total = 0;
        for (uint32_t vol : bid_volumes) {
            total += vol;
        }
        return total;
    }
};
```
*Why this is better*: Now, when iterating through `bid_volumes`, a 64-byte cache line pulls in **16** volumes at once (16 * 4 bytes). Cache misses are drastically reduced, and the compiler can easily auto-vectorize the loop using SIMD instructions.

---

**Q2. The Hot/Cold Data Split [Meta, Bloomberg]**

*Interviewer:* "We have a game engine where thousands of `GameObject` instances are updated every frame. The `update()` loop only needs the object's `position` and `velocity`. However, the object also contains heavy UI and metadata like `name`, `mesh_data`, and `description`. How do you optimize this?"

A:
This is a classic "Hot/Cold Data Splitting" scenario. 

When data is large and mostly unused during the critical loop (cold data), it bloats the struct size and evicts useful data from the L1 cache.

**Solution**: Extract the cold data into a separate dynamically allocated struct, and leave only the hot data + a pointer in the main object.

```cpp
// Cold data (rarely accessed during physics updates)
struct GameObjectMetadata {
    std::string name;
    std::string description;
    Mesh* mesh_data;
};

// Hot data (accessed 60 times a second for every object)
struct GameObject {
    Vector3 position;
    Vector3 velocity;
    
    // Pointer to cold data
    std::unique_ptr<GameObjectMetadata> meta; 
};
```
Now `GameObject` is extremely small. The physics loop can iterate through thousands of them contiguously in memory without polluting the cache with `std::string` data.

---

**Q3. Intrusive Linked Lists [Apple, Core Systems]**

*Interviewer:* "You cannot use `std::vector` because you need $\mathcal{O}(1)$ random deletions. `std::list` is too slow because iterating over it causes cache misses everywhere. How do you implement a cache-friendly linked list?"

A: Use an **Intrusive Linked List** paired with a contiguous memory pool.
In `std::list`, the container manages the node allocations. In an intrusive list, the *object itself* contains the `next` and `prev` pointers. 

You allocate all your objects upfront in a massive `std::vector` (the memory pool). You then link them together via intrusive pointers. Because they are all physically located inside the contiguous vector buffer, traversing the linked list is highly likely to hit the cache, while still allowing $\mathcal{O}(1)$ logical removals by adjusting the intrusive pointers.

---

**Q4. Polymorphism Performance Trap [Apple]**

*Interviewer:* "Why is this polymorphic array traversal slow, and how can we speed it up without dropping polymorphism?"

```cpp
std::vector<Shape*> shapes; // Contains pointers to Circles, Squares, Triangles randomly mixed
for(Shape* s : shapes) {
    s->draw(); // virtual function
}
```

A:
**Why it's slow**:
1. **Pointer Chasing**: `shapes` contains pointers. The actual `Circle` and `Square` objects are scattered randomly on the heap. Every dereference is a likely cache miss.
2. **Branch Misprediction**: Because the exact subclass changes randomly (`Circle` then `Square` then `Triangle`), the CPU cannot predict which `draw()` method in the vtable to jump to. It constantly pipeline flushes.

**How to speed it up**:
Group similar objects together! Sort the array by `typeid` or store them in separate contiguous pools.

```cpp
std::vector<Circle> circles;
std::vector<Square> squares;

for (auto& c : circles) c.draw();
for (auto& s : squares) s.draw();
```
Even if `draw()` remains a virtual function, traversing all `Circle`s consecutively means the CPU's branch predictor will achieve 99% accuracy because the destination address in the vtable remains identical for every iteration. Plus, the objects are contiguous in memory, maximizing cache hits.
