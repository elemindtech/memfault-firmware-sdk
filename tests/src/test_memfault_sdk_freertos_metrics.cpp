#include "CppUTest/MemoryLeakDetectorMallocMacros.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "comparators/comparator_memfault_metric_ids.hpp"
#include "memfault/ports/freertos/metrics.h"
#include "memfault/ports/freertos/thread_metrics.h"

static MemfaultMetricIdsComparator s_metric_id_comparator;
static uint32_t s_idle_task_run_time = 0;
static uint32_t s_total_run_time = 0;

static MemfaultMetricId idle_runtime_id = MEMFAULT_METRICS_KEY(cpu_usage_pct);
static MemfaultMetricId timer_task_stack_free_bytes_id =
  MEMFAULT_METRICS_KEY(timer_task_stack_free_bytes);

// Note: the expected value is the usage value before being scaled
// by the backend into a percent
static void prv_set_expected_usage_pct(uint32_t expected_value) {
  mock().expectOneCall("ulTaskGetIdleRunTimeCounter");
  mock().expectOneCall("portGET_RUN_TIME_COUNTER_VALUE");
  mock()
    .expectOneCall("memfault_metrics_heartbeat_set_unsigned")
    .withParameterOfType("MemfaultMetricId", "key", &idle_runtime_id)
    .withParameter("unsigned_value", expected_value);
  mock()
    .expectOneCall("memfault_metrics_heartbeat_set_unsigned")
    .withParameterOfType("MemfaultMetricId", "key", &timer_task_stack_free_bytes_id)
    .withParameter("unsigned_value", 0);
}

static void prv_set_runtimes(uint32_t idle_time, uint32_t total_time) {
  s_idle_task_run_time = idle_time;
  s_total_run_time = total_time;
}

static void prv_clear_runtimes(void) {
  prv_set_runtimes(0, 0);

  // Call to make sure previous values are set
  memfault_freertos_port_task_runtime_metrics();
}

extern "C" {
uint32_t ulTaskGetIdleRunTimeCounter(void) {
  mock().actualCall(__func__);
  return s_idle_task_run_time;
}

uint32_t portGET_RUN_TIME_COUNTER_VALUE(void) {
  mock().actualCall(__func__);
  return s_total_run_time;
}

void memfault_freertos_port_thread_metrics(void) {
  // stub
}
}

// clang-format off
TEST_GROUP(FreeRTOSMetrics) {
  void setup(){
    mock().disable();
    prv_clear_runtimes();
    mock().enable();

    mock().strictOrder();
    mock().installComparator("MemfaultMetricId", s_metric_id_comparator);
  }
  void teardown(){
    mock().removeAllComparatorsAndCopiers();
    mock().clear();
  }
};
// clang-format on

TEST(FreeRTOSMetrics, MetricNoRollover) {
  uint32_t total_runtime = 100;
  uint32_t idle_runtime = 50;
  prv_set_runtimes(idle_runtime, total_runtime);
  prv_set_expected_usage_pct(5000);
  memfault_freertos_port_task_runtime_metrics();

  total_runtime += 100;
  idle_runtime += 50;

  // Run again to ensure delta is calculated correctly
  prv_set_runtimes(idle_runtime, total_runtime);
  prv_set_expected_usage_pct(5000);
  memfault_freertos_port_task_runtime_metrics();
};

TEST(FreeRTOSMetrics, MetricRollover) {
  uint32_t total_runtime = UINT32_MAX - 1;
  uint32_t idle_runtime = UINT32_MAX / 2;

  prv_set_runtimes(idle_runtime, total_runtime);
  prv_set_expected_usage_pct(5000);
  memfault_freertos_port_task_runtime_metrics();

  total_runtime += 100;
  idle_runtime += 25;

  prv_set_runtimes(idle_runtime, total_runtime);
  prv_set_expected_usage_pct(7500);
  memfault_freertos_port_task_runtime_metrics();
};
