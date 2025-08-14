/*
* test_transforms.c - Unit Tests for MESC Clarke/Park Transformations
*
* Copyright 2024 MESC
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* Unit tests for Clarke/Park transformation accuracy and performance.
*/

#include "MESC_transforms.h"

/*============================================================================
 * TEST CONSTANTS
 *============================================================================*/

#define TEST_TOLERANCE          1e-6f   // Acceptable floating-point error
#define PI                      3.14159265359f
#define SQRT3                   1.73205080757f
#define TEST_ITERATIONS         100     // Number of test iterations

/*============================================================================
 * TEST RESULTS STORAGE
 *============================================================================*/

typedef struct {
    const char* name;
    int passed;
    float expected;
    float actual;
    float tolerance;
} test_result_t;

// Static storage for test results (embedded environment)
static test_result_t g_test_results[20];
static int g_test_count = 0;
static int g_tests_passed = 0;

/*============================================================================
 * TEST HELPER FUNCTIONS
 *============================================================================*/

// Simple sin/cos implementation for reference testing
static float sin_ref(float angle) {
    // Very basic sin approximation for testing
    float x = angle;
    if (x > PI) x -= 2*PI;
    if (x < -PI) x += 2*PI;
    return x - (x*x*x)/6.0f + (x*x*x*x*x)/120.0f;
}

static float cos_ref(float angle) {
    return sin_ref(angle + PI/2.0f);
}

static int float_equal(float a, float b, float tolerance) {
    float diff = a - b;
    return (diff > -tolerance) && (diff < tolerance);
}

static void record_test_result(const char* test_name, int passed, float expected, float actual) {
    if (g_test_count < 20) {
        g_test_results[g_test_count].name = test_name;
        g_test_results[g_test_count].passed = passed;
        g_test_results[g_test_count].expected = expected;
        g_test_results[g_test_count].actual = actual;
        g_test_results[g_test_count].tolerance = TEST_TOLERANCE;
        g_test_count++;
        if (passed) g_tests_passed++;
    }
}

/*============================================================================
 * CLARKE TRANSFORM TESTS
 *============================================================================*/

static int test_clarke_transform_balanced(void) {
    // Test with balanced three-phase currents
    float Ia = 1.0f;
    float Ib = -0.5f;
    float Ic = -0.5f;

    float alpha, beta;

    MESC_clarke_transform_optimized(Ia, Ib, Ic, &alpha, &beta, ((void*)0));

    // Expected values for balanced system
    float expected_alpha = 1.0f;  // Power-invariant scaling: alpha = Ia
    float expected_beta = 0.0f;   // Beta should be 0 for balanced 120° system

    int alpha_ok = float_equal(alpha, expected_alpha, TEST_TOLERANCE);
    int beta_ok = float_equal(beta, expected_beta, TEST_TOLERANCE);

    record_test_result("Clarke Alpha (balanced)", alpha_ok, expected_alpha, alpha);
    record_test_result("Clarke Beta (balanced)", beta_ok, expected_beta, beta);

    return alpha_ok && beta_ok;
}

static int test_clarke_transform_zero_sequence(void) {
    // Test with zero sequence component
    float Ia = 1.1f;  // +0.1 zero sequence
    float Ib = -0.4f; // +0.1 zero sequence
    float Ic = -0.4f; // +0.1 zero sequence

    float alpha, beta, gamma;

    MESC_clarke_transform_optimized(Ia, Ib, Ic, &alpha, &beta, &gamma);

    float expected_gamma = 0.1f;  // (1.1 - 0.4 - 0.4) / 3 = 0.1

    int gamma_ok = float_equal(gamma, expected_gamma, TEST_TOLERANCE);

    record_test_result("Clarke Zero Sequence", gamma_ok, expected_gamma, gamma);

    return gamma_ok;
}

/*============================================================================
 * PARK TRANSFORM TESTS
 *============================================================================*/

static int test_park_transform_zero_angle(void) {
    // Test Park transform at 0 degrees (cos=1, sin=0)
    float alpha = 1.0f;
    float beta = 0.0f;
    float cos_val = 1.0f;
    float sin_val = 0.0f;

    float d, q;

    MESC_park_transform_optimized(alpha, beta, cos_val, sin_val, &d, &q);

    // At 0°: d = alpha, q = beta
    float expected_d = alpha;
    float expected_q = beta;

    int d_ok = float_equal(d, expected_d, TEST_TOLERANCE);
    int q_ok = float_equal(q, expected_q, TEST_TOLERANCE);

    record_test_result("Park D (0°)", d_ok, expected_d, d);
    record_test_result("Park Q (0°)", q_ok, expected_q, q);

    return d_ok && q_ok;
}

static int test_park_transform_90_angle(void) {
    // Test Park transform at 90 degrees (cos=0, sin=1)
    float alpha = 1.0f;
    float beta = 0.0f;
    float cos_val = 0.0f;
    float sin_val = 1.0f;

    float d, q;

    MESC_park_transform_optimized(alpha, beta, cos_val, sin_val, &d, &q);

    // At 90°: d = beta, q = -alpha
    float expected_d = beta;
    float expected_q = -alpha;

    int d_ok = float_equal(d, expected_d, TEST_TOLERANCE);
    int q_ok = float_equal(q, expected_q, TEST_TOLERANCE);

    record_test_result("Park D (90°)", d_ok, expected_d, d);
    record_test_result("Park Q (90°)", q_ok, expected_q, q);

    return d_ok && q_ok;
}

/*============================================================================
 * INVERSE TRANSFORM TESTS
 *============================================================================*/

static int test_inverse_transforms_identity(void) {
    // Test that forward and inverse transforms are truly inverse operations
    float Ia_orig = 1.0f;
    float Ib_orig = -0.5f;
    float Ic_orig = -0.5f;

    float cos_val = cos_ref(PI/6); // 30 degrees
    float sin_val = sin_ref(PI/6);

    // Forward transform: ABC -> αβ -> dq
    float alpha, beta;
    MESC_clarke_transform_optimized(Ia_orig, Ib_orig, Ic_orig, &alpha, &beta, ((void*)0));

    float d, q;
    MESC_park_transform_optimized(alpha, beta, cos_val, sin_val, &d, &q);

    // Inverse transform: dq -> αβ -> ABC
    float alpha_inv, beta_inv;
    MESC_inv_park_transform_optimized(d, q, cos_val, sin_val, &alpha_inv, &beta_inv);

    float Va_inv, Vb_inv, Vc_inv;
    MESC_inv_clarke_transform_optimized(alpha_inv, beta_inv, &Va_inv, &Vb_inv, &Vc_inv);

    // Check that we get back the original values
    int ia_ok = float_equal(Ia_orig, Va_inv, TEST_TOLERANCE);
    int ib_ok = float_equal(Ib_orig, Vb_inv, TEST_TOLERANCE);
    int ic_ok = float_equal(Ic_orig, Vc_inv, TEST_TOLERANCE);

    record_test_result("Inverse Identity Ia", ia_ok, Ia_orig, Va_inv);
    record_test_result("Inverse Identity Ib", ib_ok, Ib_orig, Vb_inv);
    record_test_result("Inverse Identity Ic", ic_ok, Ic_orig, Vc_inv);

    return ia_ok && ib_ok && ic_ok;
}

/*============================================================================
 * PERFORMANCE TESTS
 *============================================================================*/

static int test_transformation_performance(void) {
    // Reset performance counters
    MESC_transforms_reset_performance();

    // Run multiple iterations to get stable performance measurements
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        float angle = (float)i / TEST_ITERATIONS * 2 * PI;
        float cos_val = cos_ref(angle);
        float sin_val = sin_ref(angle);

        // Test full FOC transform chain
        float Ia = sin_ref(angle);
        float Ib = sin_ref(angle - 2*PI/3);
        float Ic = sin_ref(angle + 2*PI/3);

        float Id, Iq;
        MESC_foc_transform_optimized(Ia, Ib, Ic, cos_val, sin_val, &Id, &Iq);

        float Va, Vb, Vc;
        MESC_inv_foc_transform_optimized(Id, Iq, cos_val, sin_val, &Va, &Vb, &Vc);
    }

    // Performance test always passes (it's informational)
    return 1;
}

/*============================================================================
 * MAIN TEST RUNNER
 *============================================================================*/

int test_transforms_main(void) {
    // Reset test counters
    g_test_count = 0;
    g_tests_passed = 0;

    // Initialize transform library
    MESC_transforms_init(TRANSFORM_METHOD_MANUAL);

    // Run all tests
    test_clarke_transform_balanced();
    test_clarke_transform_zero_sequence();
    test_park_transform_zero_angle();
    test_park_transform_90_angle();
    test_inverse_transforms_identity();
    test_transformation_performance();

    // Return success if all tests passed
    return (g_tests_passed == g_test_count) ? 0 : -1;
}

// Function to get test results for external reporting
int get_transform_test_results(test_result_t **results) {
    *results = g_test_results;
    return g_test_count;
}

// Function to get test summary
void get_transform_test_summary(int *total, int *passed) {
    *total = g_test_count;
    *passed = g_tests_passed;
}
