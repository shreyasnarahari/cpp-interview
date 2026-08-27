#pragma once

#include <cstdint>
#include <cstddef>
#include <immintrin.h>
#include <vector>
#include <memory>
#include <cstdlib>
#include <new>

namespace sys::simd {

/**
 * @brief High-Throughput Streaming AVX2 Vector Processing Pipeline.
 */
class VectorPipeline {
public:
    /**
     * @brief Vectorized Fused Multiply-Add with Non-Temporal Streaming Stores:
     *        dst[i] = a[i] * scale + bias
     */
    static void fma_stream(const double* __restrict src,
                           double* __restrict dst,
                           size_t count,
                           double scale,
                           double bias) noexcept {
        const __m256d v_scale = _mm256_set1_pd(scale);
        const __m256d v_bias  = _mm256_set1_pd(bias);

        size_t i = 0;
        // 4x unrolled loop: processes 16 doubles (128 bytes) per iteration
        for (; i + 15 < count; i += 16) {
            __m256d x0 = _mm256_load_pd(src + i);
            __m256d x1 = _mm256_load_pd(src + i + 4);
            __m256d x2 = _mm256_load_pd(src + i + 8);
            __m256d x3 = _mm256_load_pd(src + i + 12);

            __m256d r0 = _mm256_fmadd_pd(x0, v_scale, v_bias);
            __m256d r1 = _mm256_fmadd_pd(x1, v_scale, v_bias);
            __m256d r2 = _mm256_fmadd_pd(x2, v_scale, v_bias);
            __m256d r3 = _mm256_fmadd_pd(x3, v_scale, v_bias);

            // Streaming non-temporal stores bypassing cache
            _mm256_stream_pd(dst + i,      r0);
            _mm256_stream_pd(dst + i + 4,  r1);
            _mm256_stream_pd(dst + i + 8,  r2);
            _mm256_stream_pd(dst + i + 12, r3);
        }

        // Handle remaining blocks of 4
        for (; i + 3 < count; i += 4) {
            __m256d x = _mm256_load_pd(src + i);
            __m256d r = _mm256_fmadd_pd(x, v_scale, v_bias);
            _mm256_stream_pd(dst + i, r);
        }

        // Pipeline memory fence to flush write-combining buffers
        _mm_sfence();

        // Handle scalar tail
        for (; i < count; ++i) {
            dst[i] = src[i] * scale + bias;
        }
    }

    /**
     * @brief Vectorized Clamp / Threshold Filtering:
     *        dst[i] = max(min_val, min(max_val, src[i]))
     */
    static void clamp_stream(const double* __restrict src,
                             double* __restrict dst,
                             size_t count,
                             double min_val,
                             double max_val) noexcept {
        const __m256d v_min = _mm256_set1_pd(min_val);
        const __m256d v_max = _mm256_set1_pd(max_val);

        size_t i = 0;
        for (; i + 7 < count; i += 8) {
            __m256d x0 = _mm256_load_pd(src + i);
            __m256d x1 = _mm256_load_pd(src + i + 4);

            __m256d r0 = _mm256_max_pd(v_min, _mm256_min_pd(v_max, x0));
            __m256d r1 = _mm256_max_pd(v_min, _mm256_min_pd(v_max, x1));

            _mm256_stream_pd(dst + i,     r0);
            _mm256_stream_pd(dst + i + 4, r1);
        }

        _mm_sfence();

        for (; i < count; ++i) {
            dst[i] = (src[i] < min_val) ? min_val : ((src[i] > max_val) ? max_val : src[i]);
        }
    }
};

} // namespace sys::simd
