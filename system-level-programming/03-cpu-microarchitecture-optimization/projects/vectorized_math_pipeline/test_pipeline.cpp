#include "simd_vector_pipeline.hpp"
#include "../../exercises/market_tick.hpp"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

void test_fma_stream() {
    std::cout << "[Test 1] Testing Vector FMA Streaming ...\n";
    constexpr size_t N = 1027; // Non-multiple of 16 to test tails
    sys::simd::AlignedVector<double> src(N);
    sys::simd::AlignedVector<double> dst(N);

    for (size_t i = 0; i < N; ++i) {
        src[i] = static_cast<double>(i) * 1.5;
    }

    constexpr double scale = 2.5;
    constexpr double bias = 10.0;

    sys::simd::VectorPipeline::fma_stream(src.data(), dst.data(), N, scale, bias);

    for (size_t i = 0; i < N; ++i) {
        const double expected = src[i] * scale + bias;
        assert(std::abs(dst[i] - expected) < 1e-9);
    }
    std::cout << "  FMA Streaming calculated exact results for " << N << " elements.\n";
}

void test_clamp_stream() {
    std::cout << "[Test 2] Testing Vector Clamp Streaming ...\n";
    constexpr size_t N = 515;
    sys::simd::AlignedVector<double> src(N);
    sys::simd::AlignedVector<double> dst(N);

    for (size_t i = 0; i < N; ++i) {
        src[i] = static_cast<double>(i) - 250.0; // range: [-250, 264]
    }

    constexpr double min_val = -50.0;
    constexpr double max_val = 50.0;

    sys::simd::VectorPipeline::clamp_stream(src.data(), dst.data(), N, min_val, max_val);

    for (size_t i = 0; i < N; ++i) {
        const double expected = (src[i] < min_val) ? min_val : ((src[i] > max_val) ? max_val : src[i]);
        assert(std::abs(dst[i] - expected) < 1e-9);
    }
    std::cout << "  Clamp Streaming calculated exact results for " << N << " elements.\n";
}

int main() {
    std::cout << "=======================================================\n";
    std::cout << " Module 03: Vector Math Pipeline Verification Tests\n";
    std::cout << "=======================================================\n\n";

    test_fma_stream();
    test_clamp_stream();

    std::cout << "\n>>> ALL VECTOR PIPELINE TESTS PASSED SUCCESSFULLY <<<\n";
    return 0;
}
