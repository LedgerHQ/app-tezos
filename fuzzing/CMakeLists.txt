cmake_minimum_required(VERSION 3.13)

project(FuzzOperationParser
        VERSION 1.0
        DESCRIPTION "Fuzzing of the operation parser"
        LANGUAGES C)

# guard against bad build-type strings
if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

if (NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "Fuzzer needs to be built with Clang")
endif()

# guard against in-source builds
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there. You may need to remove CMakeCache.txt. ")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compile the swap-only parser state too, as the unit tests do
add_definitions(-DHAVE_SWAP)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# compatible with ClusterFuzzLite, which provides the sanitizer and coverage
# flags through CFLAGS and the fuzzing engine through LIB_FUZZING_ENGINE
if (NOT DEFINED ENV{LIB_FUZZING_ENGINE})
  # The parser is instrumented as well, only the fuzzer links libFuzzer
  add_compile_options(-g -O1 -fno-omit-frame-pointer
                      -fsanitize=fuzzer-no-link,address,undefined
                      -fno-sanitize-recover=all)
  add_link_options(-fsanitize=address,undefined)
  set(FUZZ_ENGINE -fsanitize=fuzzer)
else()
  separate_arguments(FUZZ_ENGINE UNIX_COMMAND "$ENV{LIB_FUZZING_ENGINE}")
endif()

set(PARSER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../app/src/parser)
set(UNIT_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../unit-tests)

# Keep in sync with PARSER_SOURCES of unit-tests/CMakeLists.txt
add_library(operation_parser STATIC
  ${PARSER_DIR}/formatting.c
  ${PARSER_DIR}/parser_state.c
  ${PARSER_DIR}/num_parser.c
  ${PARSER_DIR}/micheline_parser.c
  ${PARSER_DIR}/fa2_tokens.c
  ${PARSER_DIR}/operation_parser.c
  ${UNIT_TESTS_DIR}/strlcpy.c
  ${UNIT_TESTS_DIR}/digestif/sha256.c
)

target_include_directories(operation_parser PUBLIC
  ${PARSER_DIR}
  ${UNIT_TESTS_DIR}/digestif
)

add_executable(fuzz_operation_parser fuzz_operation_parser.c)

target_link_libraries(fuzz_operation_parser PRIVATE operation_parser)
target_link_options(fuzz_operation_parser PRIVATE ${FUZZ_ENGINE})
