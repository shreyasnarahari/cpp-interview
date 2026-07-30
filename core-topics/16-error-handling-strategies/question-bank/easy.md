# Foundational Questions: Error Handling

### Q1: When should you use exceptions vs return codes?
**Answer:** Use exceptions for unexpected/rare failure modes where normal execution cannot continue. Use return codes or `std::expected` for expected domain errors (e.g. parsing failure, cache miss, invalid user input).

### Q2: What is `std::unexpected`?
**Answer:** `std::unexpected` is a wrapper function template used to construct an error state inside a `std::expected<T, E>`.
