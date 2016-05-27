# Build all these tests with -O0, otherwise optimizations may merge some
# basic blocks and we'll fail to discover the targets.
# We change the flags for every build type because we might be doing
# a multi-configuration build (e.g. Xcode) where CMAKE_BUILD_TYPE doesn't
# mean anything.
set(variables_to_filter
  CMAKE_CXX_FLAGS_RELEASE
  CMAKE_CXX_FLAGS_DEBUG
  CMAKE_CXX_FLAGS_RELWITHDEBINFO
  CMAKE_CXX_FLAGS_MINSIZEREL
  LIBFUZZER_FLAGS_BASE
  )
foreach (VARNAME ${variables_to_filter})
  string(REPLACE " " ";" BUILD_FLAGS_AS_LIST "${${VARNAME}}")
  set(new_flags "")
  foreach (flag ${BUILD_FLAGS_AS_LIST})
    # NOTE: Use of XX here is to avoid a CMake warning due to CMP0054
    if (NOT ("XX${flag}" MATCHES "XX-O[0123s]"))
      set(new_flags "${new_flags} ${flag}")
    else()
      set(new_flags "${new_flags} -O0")
    endif()
  endforeach()
  set(${VARNAME} "${new_flags}")
endforeach()

# Enable the coverage instrumentation (it is disabled for the Fuzzer lib).
set(CMAKE_CXX_FLAGS "${LIBFUZZER_FLAGS_BASE} -fsanitize-coverage=edge,indirect-calls")

# add_libfuzzer_test(<name>
#   [NO_MAIN]
#   SOURCES source0.cpp [source1.cpp ...]
#   [EXTRA WEAK_SYMBOLS symbol0 [symbol1 ...]]
#   )
#
#   Declares a LibFuzzer test executable with target name LLVMFuzzer-<name>.
#
#   If NO_MAIN is specified the test is linked against the LibFuzzer
#   library without a main() function, otherwise it is linked against
#   the LibFuzzer library with a main() function.
#
#   One or more source files to be compiled into the binary must be declared
#   after the SOURCES keyword.
function(add_libfuzzer_test name)
  set(options "NO_MAIN")
  set(multi_arg_options "SOURCES" "EXTRA_WEAK_SYMBOLS")
  cmake_parse_arguments(
    "add_libfuzzer_test" "${options}" "" "${multi_arg_options}" ${ARGN})
  if ("${add_libfuzzer_test_SOURCES}" STREQUAL "")
    message(FATAL_ERROR "Source files must be specified")
  endif()
  add_executable(LLVMFuzzer-${name}
    ${add_libfuzzer_test_SOURCES}
    )
  if (add_libfuzzer_test_NO_MAIN)
    target_link_libraries(LLVMFuzzer-${name}
      LLVMFuzzerNoMain
      )
  else()
    target_link_libraries(LLVMFuzzer-${name}
      LLVMFuzzer
      )
  endif()
  # Place binary where llvm-lit expects to find it
  set_target_properties(LLVMFuzzer-${name}
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY
    "${CMAKE_BINARY_DIR}/lib/Fuzzer/test"
    )
  # Know LibFuzzer weak symbols
  set(weak_symbols
    "LLVMFuzzerCustomMutator"
    "LLVMFuzzerInitialize"
    ${add_libfuzzer_test_EXTRA_WEAK_SYMBOLS})
  if (APPLE)
    # On Apple platforms we need to tell the linker to allow
    # the weak symbols to be undefined
    foreach (weak_sym ${weak_symbols})
      set_property(TARGET LLVMFuzzer-${name}
        APPEND_STRING PROPERTY
        LINK_FLAGS " -Wl,-U,_${weak_sym}")
    endforeach()
  endif()
  set(TestBinaries ${TestBinaries} LLVMFuzzer-${name} PARENT_SCOPE)
endfunction()

# Variable to keep track of all test targets
set(TestBinaries)

###############################################################################
# Basic tests
###############################################################################

set(Tests
  AccumulateAllocationsTest
  BufferOverflowOnInput
  CallerCalleeTest
  CounterTest
  CustomMutatorTest
  EmptyTest
  FourIndependentBranchesTest
  FullCoverageSetTest
  InitializeTest
  MemcmpTest
  LeakTest
  LeakTimeoutTest
  NullDerefTest
  NullDerefOnEmptyTest
  NthRunCrashTest
  OutOfMemoryTest
  RepeatedMemcmp
  SimpleCmpTest
  SimpleDictionaryTest
  SimpleFnAdapterTest
  SimpleHashTest
  SimpleTest
  SpamyTest
  StrcmpTest
  StrncmpTest
  SwitchTest
  ThreadedTest
  TimeoutTest
  )

foreach(Test ${Tests})
  add_libfuzzer_test(${Test} SOURCES ${Test}.cpp)
endforeach()

if (APPLE)
  # LeakSanitizer is not supported on OSX right now
  set(HAS_LSAN 0)
else()
  set(HAS_LSAN 1)
endif()

###############################################################################
# Basic custom main function tests
###############################################################################

set(CustomMainTests
  )

foreach(Test ${CustomMainTests})
  add_libfuzzer_test(${Test} NO_MAIN SOURCES ${Test}.cpp)
endforeach()

###############################################################################
# Unit tests
###############################################################################

add_libfuzzer_test(Unittest NO_MAIN
  SOURCES
  FuzzerUnittest.cpp
  FuzzerFnAdapterUnittest.cpp
  )

target_link_libraries(LLVMFuzzer-Unittest
  gtest
  gtest_main
  )

target_include_directories(LLVMFuzzer-Unittest PRIVATE
  "${LLVM_MAIN_SRC_DIR}/utils/unittest/googletest/include"
  )

###############################################################################
# Additional tests
###############################################################################

include_directories(..)

# Test if the compiler supports the dataflow-sanitizer
# -fsanitize=dataflow
CHECK_CXX_COMPILER_FLAG("-fsanitize=address" HAS_DFSAN)
if(HAS_DFSAN)
  add_subdirectory(dfsan)
else()
  message(WARNING "Host compiler does not support the dataflow sanitizer."
    " Building and running LibFuzzer dataflow sanitizer tests is disabled."
    )
endif()

add_subdirectory(uninstrumented)
add_subdirectory(ubsan)
add_subdirectory(trace-bb)
add_subdirectory(trace-pc)

###############################################################################
# Configure lit to run the tests
#
# Note this is done after declaring all tests so we can inform lit if any tests
# need to be disabled.
###############################################################################

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
  )

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/unit/lit.site.cfg.in
  ${CMAKE_CURRENT_BINARY_DIR}/unit/lit.site.cfg
  )

add_lit_testsuite(check-fuzzer "Running Fuzzer tests"
    ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${TestBinaries} FileCheck not
    )
