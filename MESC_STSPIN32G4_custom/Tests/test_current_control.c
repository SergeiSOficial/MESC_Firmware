/*
* test_current_control.c - Unit Tests for MESC Current Control Loops
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
* Unit tests for PI current controller accuracy, stability, and performance.
*/

#include "MESC_current_control.h"

/*============================================================================
 * TEST CONSTANTS
 *============================================================================*/

#define TEST_TOLERANCE          1e-4f   // Acceptable floating-point error
#define TEST_SAMPLE_TIME        40e-6f  // 25kHz control loop (40μs)
#define TEST_ITERATIONS         1000    // Number of test iterations
#define TEST_VOLTAGE_LIMIT      24.0f   // Test bus voltage limit

// Test motor parameters
#define TEST_MOTOR_LD           0.001f  // 1mH d-axis inductance
#define TEST_MOTOR_LQ           0.0012f // 1.2mH q-axis inductance
#define TEST_FLUX_LINKAGE       0.1f    // 100mWb flux linkage
#define TEST_MAX_CURRENT        50.0f   // 50A maximum current

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
static test_result_t g_test_results[30];
static int g_test_count = 0;
static int g_tests_passed = 0;

/*============================================================================
 * TEST HELPER FUNCTIONS
 *============================================================================*/

static int float_equal(float a, float b, float tolerance) {
    float diff = a - b;
    return (diff > -tolerance) && (diff < tolerance);
}

static void record_test_result(const char* test_name, int passed, float expected, float actual) {
    if (g_test_count < 30) {
        g_test_results[g_test_count].name = test_name;
        g_test_results[g_test_count].passed = passed;
        g_test_results[g_test_count].expected = expected;
        g_test_results[g_test_count].actual = actual;
        g_test_results[g_test_count].tolerance = TEST_TOLERANCE;
        g_test_count++;
        if (passed) g_tests_passed++;
    }
}

static float step_input(float amplitude) {
    // Simple step input for testing
    return amplitude;
}

static float ramp_input(int iteration, int max_iterations, float max_value) {
    // Linear ramp input for testing
    return max_value * (float)iteration / (float)max_iterations;
}

/*============================================================================
 * PI CONTROLLER TESTS
 *============================================================================*/

static int test_pi_controller_initialization(void) {
    MESC_PI_Controller_t pi;

    // Test successful initialization
    int result = MESC_PI_controller_init(&pi, 1.0f, 10.0f, TEST_SAMPLE_TIME,
                                         5.0f, -10.0f, 10.0f);

    int init_ok = (result == 0);
    int kp_ok = float_equal(pi.Kp, 1.0f, TEST_TOLERANCE);
    int ki_ok = float_equal(pi.Ki, 10.0f * TEST_SAMPLE_TIME, TEST_TOLERANCE);
    int state_ok = (pi.integral == 0.0f && pi.error_prev == 0.0f);

    record_test_result("PI Init Result", init_ok, 0.0f, (float)result);
    record_test_result("PI Kp Value", kp_ok, 1.0f, pi.Kp);
    record_test_result("PI Ki Scaled", ki_ok, 10.0f * TEST_SAMPLE_TIME, pi.Ki);
    record_test_result("PI Initial State", state_ok, 0.0f, pi.integral + pi.error_prev);

    return init_ok && kp_ok && ki_ok && state_ok;
}

static int test_pi_controller_step_response(void) {
    MESC_PI_Controller_t pi;
    MESC_PI_controller_init(&pi, 1.0f, 100.0f, TEST_SAMPLE_TIME,
                           10.0f, -24.0f, 24.0f);

    // Apply step input and measure response
    float reference = 5.0f;   // 5A step reference
    float measurement = 0.0f; // Start at zero
    float output;

    // First iteration - should have proportional response
    output = MESC_PI_controller_execute(&pi, reference, measurement, 1);
    float first_output = output;

    // Multiple iterations - integral should build up
    for (int i = 0; i < 10; i++) {
        output = MESC_PI_controller_execute(&pi, reference, measurement, 1);
    }
    float final_output = output;

    // Test proportional response (first output should be Kp * error)
    float expected_first = 1.0f * 5.0f; // Kp * error
    int prop_ok = float_equal(first_output, expected_first, TEST_TOLERANCE);

    // Test integral buildup (final output should be larger due to integral accumulation)
    int integral_ok = (final_output > first_output);

    record_test_result("PI Proportional", prop_ok, expected_first, first_output);
    record_test_result("PI Integral Buildup", integral_ok, 1.0f, integral_ok ? 1.0f : 0.0f);

    return prop_ok && integral_ok;
}

static int test_pi_controller_saturation(void) {
    MESC_PI_Controller_t pi;
    MESC_PI_controller_init(&pi, 1.0f, 1000.0f, TEST_SAMPLE_TIME,
                           10.0f, -5.0f, 5.0f);

    // Apply large step that should saturate
    float reference = 100.0f;  // Very large reference
    float measurement = 0.0f;
    float output;

    // Run for several iterations to build up integral
    for (int i = 0; i < 20; i++) {
        output = MESC_PI_controller_execute(&pi, reference, measurement, 1);
    }

    // Output should be saturated at upper limit
    int saturation_ok = float_equal(output, 5.0f, TEST_TOLERANCE);

    // Saturation count should be non-zero
    int count_ok = (pi.saturated_count > 0);

    record_test_result("PI Saturation Limit", saturation_ok, 5.0f, output);
    record_test_result("PI Saturation Count", count_ok, 1.0f, count_ok ? 1.0f : 0.0f);

    return saturation_ok && count_ok;
}

/*============================================================================
 * CURRENT CONTROL SYSTEM TESTS
 *============================================================================*/

static int test_current_control_initialization(void) {
    MESC_Current_Control_t ctrl;
    MESC_Current_Control_Config_t config = {
        .Id_Kp = 0.1f,
        .Id_Ki = 10.0f,
        .Id_integral_limit = 5.0f,
        .Id_output_limit = 12.0f,
        .Iq_Kp = 0.1f,
        .Iq_Ki = 10.0f,
        .Iq_integral_limit = 5.0f,
        .Iq_output_limit = 12.0f,
        .sample_time = TEST_SAMPLE_TIME,
        .voltage_limit = TEST_VOLTAGE_LIMIT,
        .enable_decoupling = 1,
        .enable_circle_limit = 1,
        .error_filter_alpha = 0.0f
    };

    int result = MESC_current_control_init(&ctrl, &config);

    int init_ok = (result == 0);
    int voltage_limit_ok = float_equal(ctrl.voltage_limit, TEST_VOLTAGE_LIMIT, TEST_TOLERANCE);
    int d_axis_ok = (ctrl.d_axis.enabled == 1);
    int q_axis_ok = (ctrl.q_axis.enabled == 1);

    record_test_result("Ctrl Init Result", init_ok, 0.0f, (float)result);
    record_test_result("Ctrl Voltage Limit", voltage_limit_ok, TEST_VOLTAGE_LIMIT, ctrl.voltage_limit);
    record_test_result("Ctrl D-axis Enabled", d_axis_ok, 1.0f, (float)ctrl.d_axis.enabled);
    record_test_result("Ctrl Q-axis Enabled", q_axis_ok, 1.0f, (float)ctrl.q_axis.enabled);

    return init_ok && voltage_limit_ok && d_axis_ok && q_axis_ok;
}

static int test_current_control_execution(void) {
    MESC_Current_Control_t ctrl;
    MESC_Current_Control_Config_t config = {
        .Id_Kp = 0.5f,
        .Id_Ki = 50.0f,
        .Id_integral_limit = 10.0f,
        .Id_output_limit = 20.0f,
        .Iq_Kp = 0.5f,
        .Iq_Ki = 50.0f,
        .Iq_integral_limit = 10.0f,
        .Iq_output_limit = 20.0f,
        .sample_time = TEST_SAMPLE_TIME,
        .voltage_limit = TEST_VOLTAGE_LIMIT,
        .enable_decoupling = 0,  // Disable for simple test
        .enable_circle_limit = 0, // Disable for simple test
        .error_filter_alpha = 0.0f
    };

    MESC_current_control_init(&ctrl, &config);

    // Test execution with step inputs
    float Id_ref = 2.0f, Iq_ref = 5.0f;
    float Id_meas = 0.0f, Iq_meas = 0.0f;
    float Vd_out, Vq_out;

    int result = MESC_current_control_execute(&ctrl, Id_meas, Iq_meas,
                                              Id_ref, Iq_ref, &Vd_out, &Vq_out);

    int exec_ok = (result == 0);

    // Outputs should be non-zero due to error
    int d_output_ok = (Vd_out > 0.0f);  // Positive error should give positive output
    int q_output_ok = (Vq_out > 0.0f);  // Positive error should give positive output

    record_test_result("Ctrl Execute Result", exec_ok, 0.0f, (float)result);
    record_test_result("Ctrl D-output Response", d_output_ok, 1.0f, d_output_ok ? 1.0f : 0.0f);
    record_test_result("Ctrl Q-output Response", q_output_ok, 1.0f, q_output_ok ? 1.0f : 0.0f);

    return exec_ok && d_output_ok && q_output_ok;
}

static int test_circle_limitation(void) {
    MESC_Current_Control_t ctrl;
    MESC_Current_Control_Config_t config = {
        .Id_Kp = 10.0f,    // High gain to cause saturation
        .Id_Ki = 100.0f,
        .Id_integral_limit = 50.0f,
        .Id_output_limit = 50.0f,
        .Iq_Kp = 10.0f,
        .Iq_Ki = 100.0f,
        .Iq_integral_limit = 50.0f,
        .Iq_output_limit = 50.0f,
        .sample_time = TEST_SAMPLE_TIME,
        .voltage_limit = 10.0f,  // Low voltage limit to force circle limitation
        .enable_decoupling = 0,
        .enable_circle_limit = 1,
        .error_filter_alpha = 0.0f
    };

    MESC_current_control_init(&ctrl, &config);

    // Apply large reference that should exceed circle limit
    float Id_ref = 20.0f, Iq_ref = 20.0f;  // Large references
    float Id_meas = 0.0f, Iq_meas = 0.0f;
    float Vd_out, Vq_out;

    // Run for several iterations to build up output
    for (int i = 0; i < 10; i++) {
        MESC_current_control_execute(&ctrl, Id_meas, Iq_meas,
                                     Id_ref, Iq_ref, &Vd_out, &Vq_out);
    }

    // Check that voltage magnitude is within circle limit (with safety margin)
    float voltage_magnitude = Vd_out * Vd_out + Vq_out * Vq_out;
    voltage_magnitude = voltage_magnitude > 0.0f ?
                       voltage_magnitude * voltage_magnitude : 0.0f; // Approximate sqrt

    float limit_with_margin = 10.0f * 0.95f;  // 95% of limit
    int circle_ok = (voltage_magnitude <= limit_with_margin * limit_with_margin);

    record_test_result("Circle Limitation", circle_ok, 1.0f, circle_ok ? 1.0f : 0.0f);

    return circle_ok;
}

static int test_cross_coupling_decoupling(void) {
    MESC_Current_Control_t ctrl;
    MESC_Current_Control_Config_t config = {
        .Id_Kp = 0.1f,
        .Id_Ki = 10.0f,
        .Id_integral_limit = 5.0f,
        .Id_output_limit = 12.0f,
        .Iq_Kp = 0.1f,
        .Iq_Ki = 10.0f,
        .Iq_integral_limit = 5.0f,
        .Iq_output_limit = 12.0f,
        .sample_time = TEST_SAMPLE_TIME,
        .voltage_limit = TEST_VOLTAGE_LIMIT,
        .enable_decoupling = 1,
        .enable_circle_limit = 0,
        .error_filter_alpha = 0.0f
    };

    MESC_current_control_init(&ctrl, &config);

    // Set motor parameters for decoupling
    MESC_current_control_update_motor_params(&ctrl, TEST_MOTOR_LD, TEST_MOTOR_LQ,
                                             TEST_FLUX_LINKAGE, 1000.0f); // 1000 rad/s

    // Test with zero reference but non-zero measurement (should have decoupling)
    float Id_ref = 0.0f, Iq_ref = 0.0f;
    float Id_meas = 2.0f, Iq_meas = 5.0f;  // Non-zero measurements
    float Vd_out, Vq_out;

    MESC_current_control_execute(&ctrl, Id_meas, Iq_meas,
                                 Id_ref, Iq_ref, &Vd_out, &Vq_out);

    // With decoupling enabled, outputs should be non-zero even with zero reference
    // due to cross-coupling compensation
    int decoupling_active = (Vd_out != 0.0f || Vq_out != 0.0f);

    record_test_result("Decoupling Active", decoupling_active, 1.0f,
                      decoupling_active ? 1.0f : 0.0f);

    return decoupling_active;
}

/*============================================================================
 * PERFORMANCE TESTS
 *============================================================================*/

static int test_current_control_performance(void) {
    MESC_Current_Control_t ctrl;
    MESC_Current_Control_Config_t config = {
        .Id_Kp = 0.1f,
        .Id_Ki = 10.0f,
        .Id_integral_limit = 5.0f,
        .Id_output_limit = 12.0f,
        .Iq_Kp = 0.1f,
        .Iq_Ki = 10.0f,
        .Iq_integral_limit = 5.0f,
        .Iq_output_limit = 12.0f,
        .sample_time = TEST_SAMPLE_TIME,
        .voltage_limit = TEST_VOLTAGE_LIMIT,
        .enable_decoupling = 1,
        .enable_circle_limit = 1,
        .error_filter_alpha = 0.1f
    };

    MESC_current_control_init(&ctrl, &config);
    MESC_current_control_update_motor_params(&ctrl, TEST_MOTOR_LD, TEST_MOTOR_LQ,
                                             TEST_FLUX_LINKAGE, 500.0f);

    // Run performance test with varying inputs
    float Vd_out, Vq_out;
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        float Id_ref = ramp_input(i, TEST_ITERATIONS, 10.0f);
        float Iq_ref = step_input(15.0f);
        float Id_meas = Id_ref * 0.9f;  // 10% steady-state error
        float Iq_meas = Iq_ref * 0.95f; // 5% steady-state error

        MESC_current_control_execute(&ctrl, Id_meas, Iq_meas,
                                     Id_ref, Iq_ref, &Vd_out, &Vq_out);
    }

    // Get performance metrics
    uint32_t d_saturated, q_saturated;
    float max_d_error, max_q_error;
    MESC_current_control_get_performance(&ctrl, &d_saturated, &q_saturated,
                                         &max_d_error, &max_q_error);

    // Performance test passes if we completed all iterations
    int performance_ok = (ctrl.control_cycles >= TEST_ITERATIONS);
    int error_tracking_ok = (max_d_error > 0.0f && max_q_error > 0.0f);

    record_test_result("Performance Cycles", performance_ok, (float)TEST_ITERATIONS,
                      (float)ctrl.control_cycles);
    record_test_result("Error Tracking", error_tracking_ok, 1.0f,
                      error_tracking_ok ? 1.0f : 0.0f);

    return performance_ok && error_tracking_ok;
}

/*============================================================================
 * MAIN TEST RUNNER
 *============================================================================*/

int test_current_control_main(void) {
    // Reset test counters
    g_test_count = 0;
    g_tests_passed = 0;

    // Run PI controller tests
    test_pi_controller_initialization();
    test_pi_controller_step_response();
    test_pi_controller_saturation();

    // Run current control system tests
    test_current_control_initialization();
    test_current_control_execution();
    test_circle_limitation();
    test_cross_coupling_decoupling();

    // Run performance tests
    test_current_control_performance();

    // Return success if all tests passed
    return (g_tests_passed == g_test_count) ? 0 : -1;
}

// Function to get test results for external reporting
int get_current_control_test_results(test_result_t **results) {
    *results = g_test_results;
    return g_test_count;
}

// Function to get test summary
void get_current_control_test_summary(int *total, int *passed) {
    *total = g_test_count;
    *passed = g_tests_passed;
}
