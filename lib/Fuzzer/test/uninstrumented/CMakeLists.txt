# These tests are not instrumented with coverage.

set(CMAKE_CXX_FLAGS
  "${LIBFUZZER_FLAGS_BASE} -fno-sanitize=all -fno-sanitize-coverage=edge,trace-cmp,indirect-calls,8bit-counters")

set(UninstrumentedTests
  UninstrumentedTest
  )

set(WEAK_SYMBOLS
  __lsan_disable
  __lsan_do_recoverable_leak_check
  __lsan_enable
  __sanitizer_get_coverage_pc_buffer
  __sanitizer_get_number_of_counters
  __sanitizer_get_total_unique_caller_callee_pairs
  __sanitizer_get_total_unique_coverage
  __sanitizer_print_stack_trace
  __sanitizer_reset_coverage
  __sanitizer_set_death_callback
  __sanitizer_update_counter_bitset_and_clear_counters
)

foreach(Test ${UninstrumentedTests})
  add_libfuzzer_test(${Test}-Uninstrumented
    SOURCES ../${Test}.cpp
    EXTRA_WEAK_SYMBOLS ${WEAK_SYMBOLS}
    )
endforeach()

# Propagate value into parent directory
set(TestBinaries ${TestBinaries} PARENT_SCOPE)
