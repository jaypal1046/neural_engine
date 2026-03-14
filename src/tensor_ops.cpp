// High-Performance Tensor Operations
// Algorithm adapted from: GGML (MIT License)
// Original: https://github.com/ggerganov/llama.cpp/blob/master/ggml.c
// Modifications: Removed dependencies, optimized for AIZip brain

#include "tensor_ops.h"
#include "thread_pool.h"
#include <cstring>
#include <algorithm>
#include <cmath>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif

#ifdef __AVX2__
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace TensorOps {

// ============================================================================
// CPU Feature Detection
// ============================================================================

CPUFeatures detect_cpu_features() {
    CPUFeatures features = {};

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    int info[4];

#ifdef _MSC_VER
    __cpuid(info, 1);
#else
    __cpuid(1, info[0], info[1], info[2], info[3]);
#endif

    features.sse2 = (info[3] & (1 << 26)) != 0;
    features.sse42 = (info[2] & (1 << 20)) != 0;
    features.avx = (info[2] & (1 << 28)) != 0;
    features.fma = (info[2] & (1 << 12)) != 0;

    // Check AVX2
#ifdef _MSC_VER
    __cpuidex(info, 7, 0);
#else
    __cpuid_count(7, 0, info[0], info[1], info[2], info[3]);
#endif
    features.avx2 = (info[1] & (1 << 5)) != 0;
    features.avx512 = (info[1] & (1 << 16)) != 0;

#elif defined(__ARM_NEON)
    features.neon = true;
#endif

    return features;
}

static CPUFeatures g_cpu_features = detect_cpu_features();

// ============================================================================
// Scalar Implementations (Fallback)
// ============================================================================

namespace internal {

float vec_dot_scalar(const float* a, const float* b, int n) {
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

void matmul_scalar(const float* A, const float* B, float* C, int m, int k, int n) {
    // C[m,n] = A[m,k] @ B[k,n]
    // Parallelize outer loops with thread pool
    ThreadPool::parallel_for_2d(m, n, [&](int i, int j) {
        float sum = 0.0f;
        for (int p = 0; p < k; p++) {
            sum += A[i * k + p] * B[p * n + j];
        }
        C[i * n + j] = sum;
    });
}

// ============================================================================
// SSE2 Implementations (Guaranteed on x86_64)
// ============================================================================

#if defined(__SSE2__) || defined(_M_X64)

float vec_dot_sse2(const float* a, const float* b, int n) {
    __m128 sum_vec = _mm_setzero_ps();

    int i = 0;
    // Process 4 floats at a time
    for (; i + 3 < n; i += 4) {
        __m128 a_vec = _mm_loadu_ps(&a[i]);
        __m128 b_vec = _mm_loadu_ps(&b[i]);
        sum_vec = _mm_add_ps(sum_vec, _mm_mul_ps(a_vec, b_vec));
    }

    // Horizontal sum
    float result[4];
    _mm_storeu_ps(result, sum_vec);
    float sum = result[0] + result[1] + result[2] + result[3];

    // Handle remaining elements
    for (; i < n; i++) {
        sum += a[i] * b[i];
    }

    return sum;
}

void matmul_sse2(const float* A, const float* B, float* C, int m, int k, int n) {
    // Optimized matrix multiplication with SSE2
    // Uses row-major order and vectorizes the inner loop
    // Parallelize with thread pool for multi-core scaling

    ThreadPool::parallel_for_2d(m, n, [&](int i, int j) {
        __m128 sum_vec = _mm_setzero_ps();

        int p = 0;
        // Process 4 elements at a time
        for (; p + 3 < k; p += 4) {
            __m128 a_vec = _mm_loadu_ps(&A[i * k + p]);
            __m128 b_vec = _mm_set_ps(
                B[(p + 3) * n + j],
                B[(p + 2) * n + j],
                B[(p + 1) * n + j],
                B[p * n + j]
            );
            sum_vec = _mm_add_ps(sum_vec, _mm_mul_ps(a_vec, b_vec));
        }

        // Horizontal sum
        float result[4];
        _mm_storeu_ps(result, sum_vec);
        float sum = result[0] + result[1] + result[2] + result[3];

        // Handle remaining elements
        for (; p < k; p++) {
            sum += A[i * k + p] * B[p * n + j];
        }

        C[i * n + j] = sum;
    });
}

#else
// Fallback to scalar if SSE2 not available
float vec_dot_sse2(const float* a, const float* b, int n) {
    return vec_dot_scalar(a, b, n);
}

void matmul_sse2(const float* A, const float* B, float* C, int m, int k, int n) {
    matmul_scalar(A, B, C, m, k, n);
}
#endif

// ============================================================================
// AVX2 Implementations (Runtime Detection)
// ============================================================================

#ifdef __AVX2__

float vec_dot_avx2(const float* a, const float* b, int n) {
    __m256 sum_vec = _mm256_setzero_ps();

    int i = 0;
    // Process 8 floats at a time
    for (; i + 7 < n; i += 8) {
        __m256 a_vec = _mm256_loadu_ps(&a[i]);
        __m256 b_vec = _mm256_loadu_ps(&b[i]);
        sum_vec = _mm256_add_ps(sum_vec, _mm256_mul_ps(a_vec, b_vec));
    }

    // Horizontal sum
    float result[8];
    _mm256_storeu_ps(result, sum_vec);
    float sum = result[0] + result[1] + result[2] + result[3] +
                result[4] + result[5] + result[6] + result[7];

    // Handle remaining elements
    for (; i < n; i++) {
        sum += a[i] * b[i];
    }

    return sum;
}

void matmul_avx2(const float* A, const float* B, float* C, int m, int k, int n) {
    // High-performance matrix multiplication with AVX2
    // Algorithm from GGML: uses blocking and vectorization
    // Thread pool parallelization for multi-core CPUs

    const int BLOCK_SIZE = 32;  // Cache-friendly block size

    // Initialize output to zero
    std::fill_n(C, m * n, 0.0f);

    // Blocked matrix multiplication with outer loop parallelization
    int num_blocks = (m + BLOCK_SIZE - 1) / BLOCK_SIZE;
    ThreadPool::parallel_for(0, num_blocks, [&](int block_idx) {
        int i0 = block_idx * BLOCK_SIZE;
        for (int j0 = 0; j0 < n; j0 += BLOCK_SIZE) {
            for (int p0 = 0; p0 < k; p0 += BLOCK_SIZE) {

                int i_max = std::min(i0 + BLOCK_SIZE, m);
                int j_max = std::min(j0 + BLOCK_SIZE, n);
                int p_max = std::min(p0 + BLOCK_SIZE, k);

                // Process block
                for (int i = i0; i < i_max; i++) {
                    for (int j = j0; j < j_max; j++) {
                        __m256 sum_vec = _mm256_setzero_ps();

                        int p = p0;
                        // Vectorized inner loop
                        for (; p + 7 < p_max; p += 8) {
                            __m256 a_vec = _mm256_loadu_ps(&A[i * k + p]);
                            __m256 b_vec = _mm256_set_ps(
                                B[(p + 7) * n + j],
                                B[(p + 6) * n + j],
                                B[(p + 5) * n + j],
                                B[(p + 4) * n + j],
                                B[(p + 3) * n + j],
                                B[(p + 2) * n + j],
                                B[(p + 1) * n + j],
                                B[p * n + j]
                            );
                            sum_vec = _mm256_add_ps(sum_vec, _mm256_mul_ps(a_vec, b_vec));
                        }

                        // Horizontal sum
                        float result[8];
                        _mm256_storeu_ps(result, sum_vec);
                        float sum = result[0] + result[1] + result[2] + result[3] +
                                    result[4] + result[5] + result[6] + result[7];

                        // Handle remaining elements
                        for (; p < p_max; p++) {
                            sum += A[i * k + p] * B[p * n + j];
                        }

                        C[i * n + j] += sum;
                    }
                }
            }
        }
    });
}

#else
// Fallback to SSE2 if AVX2 not available
float vec_dot_avx2(const float* a, const float* b, int n) {
    return vec_dot_sse2(a, b, n);
}

void matmul_avx2(const float* A, const float* B, float* C, int m, int k, int n) {
    matmul_sse2(A, B, C, m, k, n);
}
#endif

// AVX-512 placeholders (future optimization)
float vec_dot_avx512(const float* a, const float* b, int n) {
    return vec_dot_avx2(a, b, n);  // Fall back to AVX2 for now
}

void matmul_avx512(const float* A, const float* B, float* C, int m, int k, int n) {
    matmul_avx2(A, B, C, m, k, n);  // Fall back to AVX2 for now
}

} // namespace internal

// ============================================================================
// Public API (Auto-dispatch to best SIMD version)
// ============================================================================

float vec_dot(const float* a, const float* b, int n) {
    if (g_cpu_features.avx2) {
        return internal::vec_dot_avx2(a, b, n);
    } else if (g_cpu_features.sse2) {
        return internal::vec_dot_sse2(a, b, n);
    } else {
        return internal::vec_dot_scalar(a, b, n);
    }
}

void matmul(const float* A, const float* B, float* C, int m, int k, int n) {
    if (g_cpu_features.avx2) {
        internal::matmul_avx2(A, B, C, m, k, n);
    } else if (g_cpu_features.sse2) {
        internal::matmul_sse2(A, B, C, m, k, n);
    } else {
        internal::matmul_scalar(A, B, C, m, k, n);
    }
}

void matmul_transposed(const float* A, const float* B, float* C, int m, int k, int n) {
    // C[m,n] = A[m,k] @ B[n,k]^T
    // B is stored transposed, so B[n,k] means row-major access is cache-friendly
    // Parallelize outer loops with thread pool
    ThreadPool::parallel_for_2d(m, n, [&](int i, int j) {
        C[i * n + j] = vec_dot(&A[i * k], &B[j * k], k);
    });
}

void matvec(const float* A, const float* x, float* y, int m, int n) {
    // y[m] = A[m,n] @ x[n]
    // Parallelize over output rows with thread pool
    ThreadPool::parallel_for_static(0, m, [&](int i) {
        y[i] = vec_dot(&A[i * n], x, n);
    });
}

// ============================================================================
// Element-wise Operations
// ============================================================================

void vec_add(float* dst, const float* a, const float* b, int n) {
#ifdef __AVX2__
    if (g_cpu_features.avx2) {
        int i = 0;
        for (; i + 7 < n; i += 8) {
            __m256 a_vec = _mm256_loadu_ps(&a[i]);
            __m256 b_vec = _mm256_loadu_ps(&b[i]);
            __m256 result = _mm256_add_ps(a_vec, b_vec);
            _mm256_storeu_ps(&dst[i], result);
        }
        for (; i < n; i++) {
            dst[i] = a[i] + b[i];
        }
        return;
    }
#endif
    for (int i = 0; i < n; i++) {
        dst[i] = a[i] + b[i];
    }
}

void vec_sub(float* dst, const float* a, const float* b, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = a[i] - b[i];
    }
}

void vec_mul(float* dst, const float* a, const float* b, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = a[i] * b[i];
    }
}

void vec_scale(float* dst, const float* src, float scale, int n) {
#ifdef __AVX2__
    if (g_cpu_features.avx2) {
        __m256 scale_vec = _mm256_set1_ps(scale);
        int i = 0;
        for (; i + 7 < n; i += 8) {
            __m256 src_vec = _mm256_loadu_ps(&src[i]);
            __m256 result = _mm256_mul_ps(src_vec, scale_vec);
            _mm256_storeu_ps(&dst[i], result);
        }
        for (; i < n; i++) {
            dst[i] = src[i] * scale;
        }
        return;
    }
#endif
    for (int i = 0; i < n; i++) {
        dst[i] = src[i] * scale;
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

void copy(float* dst, const float* src, int n) {
    std::memcpy(dst, src, n * sizeof(float));
}

void fill(float* dst, float value, int n) {
    std::fill_n(dst, n, value);
}

float l2_norm(const float* x, int n) {
    float sum_sq = vec_dot(x, x, n);
    return std::sqrt(sum_sq);
}

void clip_by_norm(float* grad, int n, float max_norm) {
    float norm = l2_norm(grad, n);
    if (norm > max_norm) {
        float scale = max_norm / norm;
        vec_scale(grad, grad, scale, n);
    }
}

// ============================================================================
// Activation Functions
// ============================================================================

void gelu(float* out, const float* x, int n) {
    const float sqrt_2_over_pi = 0.7978845608f;  // sqrt(2/pi)
    const float coeff = 0.044715f;

    for (int i = 0; i < n; i++) {
        float xi = x[i];
        float cube = xi * xi * xi;
        float inner = sqrt_2_over_pi * (xi + coeff * cube);
        out[i] = 0.5f * xi * (1.0f + std::tanh(inner));
    }
}

void softmax(float* out, const float* x, int n) {
    // Find max for numerical stability
    float max_val = x[0];
    for (int i = 1; i < n; i++) {
        if (x[i] > max_val) max_val = x[i];
    }

    // Compute exp and sum
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        out[i] = std::exp(x[i] - max_val);
        sum += out[i];
    }

    // Normalize
    float inv_sum = 1.0f / sum;
    for (int i = 0; i < n; i++) {
        out[i] *= inv_sum;
    }
}

void rmsnorm(float* out, const float* x, const float* weight, int n, float eps) {
    // Compute RMS
    float sum_sq = 0.0f;
    for (int i = 0; i < n; i++) {
        sum_sq += x[i] * x[i];
    }
    float rms = std::sqrt(sum_sq / n + eps);
    float inv_rms = 1.0f / rms;

    // Normalize and scale
    for (int i = 0; i < n; i++) {
        out[i] = x[i] * inv_rms * weight[i];
    }
}

} // namespace TensorOps
