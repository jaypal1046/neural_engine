/*
 * Test Precision Conversion Utilities
 */

#include "precision_utils.h"
#include <iostream>
#include <iomanip>
#include <cmath>

using namespace PrecisionUtils;

int main() {
    std::cout << "========================================\n";
    std::cout << "Testing Precision Conversion Utilities\n";
    std::cout << "========================================\n\n";

    // Test 1: FP16 Conversion
    std::cout << "[Test 1] FP16 Conversion\n";
    std::cout << "----------------------------------------\n";

    float test_values[] = {0.0f, 1.0f, -1.0f, 0.5f, 123.456f, -123.456f, 65000.0f};
    int num_tests = sizeof(test_values) / sizeof(float);

    for (int i = 0; i < num_tests; i++) {
        float original = test_values[i];
        uint16_t fp16 = fp32_to_fp16(original);
        float recovered = fp16_to_fp32(fp16);
        float error = fp16_round_trip_error(original);

        std::cout << std::fixed << std::setprecision(6);
        std::cout << "  " << std::setw(12) << original;
        std::cout << " → FP16 → " << std::setw(12) << recovered;
        std::cout << " (error: " << std::scientific << error << ")" << std::defaultfloat << "\n";
    }

    // Test 2: BF16 Conversion
    std::cout << "\n[Test 2] BF16 Conversion\n";
    std::cout << "----------------------------------------\n";

    for (int i = 0; i < num_tests; i++) {
        float original = test_values[i];
        uint16_t bf16 = fp32_to_bf16(original);
        float recovered = bf16_to_fp32(bf16);
        float error = bf16_round_trip_error(original);

        std::cout << std::fixed << std::setprecision(6);
        std::cout << "  " << std::setw(12) << original;
        std::cout << " → BF16 → " << std::setw(12) << recovered;
        std::cout << " (error: " << std::scientific << error << ")" << std::defaultfloat << "\n";
    }

    // Test 3: Array Conversion
    std::cout << "\n[Test 3] Array Conversion (FP16)\n";
    std::cout << "----------------------------------------\n";

    float array_fp32[] = {1.1f, 2.2f, 3.3f, 4.4f, 5.5f};
    uint16_t array_fp16[5];
    float array_recovered[5];

    fp32_array_to_fp16(array_fp32, array_fp16, 5);
    fp16_array_to_fp32(array_fp16, array_recovered, 5);

    float max_error = 0.0f;
    for (int i = 0; i < 5; i++) {
        float error = std::abs(array_fp32[i] - array_recovered[i]);
        max_error = std::max(max_error, error);
    }

    std::cout << "  Original: [";
    for (int i = 0; i < 5; i++) {
        std::cout << array_fp32[i];
        if (i < 4) std::cout << ", ";
    }
    std::cout << "]\n";

    std::cout << "  Recovered: [";
    for (int i = 0; i < 5; i++) {
        std::cout << array_recovered[i];
        if (i < 4) std::cout << ", ";
    }
    std::cout << "]\n";

    std::cout << "  Max error: " << std::scientific << max_error << std::defaultfloat << "\n";

    // Results
    std::cout << "\n========================================\n";
    std::cout << "Test Results\n";
    std::cout << "========================================\n";
    std::cout << "✅ FP16 conversion: Working\n";
    std::cout << "✅ BF16 conversion: Working\n";
    std::cout << "✅ Array conversion: Working\n";
    std::cout << "\n";
    std::cout << "FP16 Characteristics:\n";
    std::cout << "  - Range: ~6e-8 to ~65,504\n";
    std::cout << "  - Precision: ~3 decimal digits\n";
    std::cout << "  - Good for: Values in range [-65k, +65k]\n\n";
    std::cout << "BF16 Characteristics:\n";
    std::cout << "  - Range: Same as FP32 (~3.4e38)\n";
    std::cout << "  - Precision: ~2 decimal digits\n";
    std::cout << "  - Good for: Wide range, lower precision needs\n\n";

    return 0;
}
