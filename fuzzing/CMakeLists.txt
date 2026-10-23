cmake_minimum_required(VERSION 3.13)

project(FuzzTezos
        VERSION 1.0
        DESCRIPTION "Fuzzing of the parsers and helpers of the Tezos app"
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
  # The code under test is instrumented as well, only the fuzzers link libFuzzer
  # The sanitizers of the framework fuzzers (see LedgerAppFuzz of the SDK), so
  # that what one reports is found here, in seconds, and not in a campaign
  set(FUZZ_SANITIZE
      fuzzer-no-link,address,undefined,integer,nullability,local-bounds,float-divide-by-zero)
  add_compile_options(-g -O1 -fno-omit-frame-pointer
                      -fsanitize=${FUZZ_SANITIZE}
                      -fno-sanitize=alignment
                      -fno-sanitize-recover=all
                      -fsanitize-ignorelist=${CMAKE_CURRENT_SOURCE_DIR}/../app/fuzzing/ubsan-ignorelist.txt)
  add_link_options(-fsanitize=address,undefined,integer)
  set(FUZZ_ENGINE -fsanitize=fuzzer)
else()
  separate_arguments(FUZZ_ENGINE UNIX_COMMAND "$ENV{LIB_FUZZING_ENGINE}")
endif()

set(APP_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../app/src)
set(PARSER_DIR ${APP_SRC_DIR}/parser)
set(UNIT_TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../unit-tests)
# The checks of the targets are shared with the ones of app/fuzzing
set(CHECKS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../app/fuzzing/checks)

# The parsers and the helpers they share, but for the operation parser: the
# fuzzer of the amounts includes its source, to reach what is static in it.
# Keep in sync with PARSER_SOURCES of unit-tests/CMakeLists.txt
add_library(parser_common STATIC
  ${PARSER_DIR}/formatting.c
  ${PARSER_DIR}/parser_state.c
  ${PARSER_DIR}/num_parser.c
  ${PARSER_DIR}/micheline_parser.c
  ${PARSER_DIR}/fa2_tokens.c
  ${UNIT_TESTS_DIR}/strlcpy.c
  ${UNIT_TESTS_DIR}/digestif/sha256.c
)

target_include_directories(parser_common PUBLIC
  ${PARSER_DIR}
  ${UNIT_TESTS_DIR}/digestif
)

add_library(operation_parser STATIC ${PARSER_DIR}/operation_parser.c)

target_link_libraries(operation_parser PUBLIC parser_common)

# fuzz_<name> from the given sources
function(add_fuzzer name)
  cmake_parse_arguments(F "" "" "SOURCES;LIBRARIES;INCLUDES;DEFINITIONS" ${ARGN})

  add_executable(fuzz_${name} ${F_SOURCES})
  target_include_directories(fuzz_${name} PRIVATE ${CHECKS_DIR})
  if (F_LIBRARIES)
    target_link_libraries(fuzz_${name} PRIVATE ${F_LIBRARIES})
  endif()
  if (F_INCLUDES)
    target_include_directories(fuzz_${name} PRIVATE ${F_INCLUDES})
  endif()
  if (F_DEFINITIONS)
    target_compile_definitions(fuzz_${name} PRIVATE ${F_DEFINITIONS})
  endif()
  target_link_options(fuzz_${name} PRIVATE ${FUZZ_ENGINE})
endfunction()

add_fuzzer(operation_parser
  SOURCES fuzz_operation_parser.c
  LIBRARIES operation_parser)

add_fuzzer(micheline_parser
  SOURCES fuzz_micheline_parser.c
  LIBRARIES parser_common)

add_fuzzer(formatting
  SOURCES fuzz_formatting.c
  LIBRARIES parser_common)

add_fuzzer(amounts
  SOURCES fuzz_amounts.c
  LIBRARIES parser_common)

add_fuzzer(fa2_tokens
  SOURCES fuzz_fa2_tokens.c
  LIBRARIES parser_common)

add_fuzzer(swap_key_policy
  SOURCES fuzz_swap_key_policy.c ${APP_SRC_DIR}/swap_key_policy.c
  INCLUDES ${APP_SRC_DIR})

# The ring buffer comes in the size of each kind of device. The shims stand in
# for the SDK headers, as in the unit tests, and must come first.
function(add_ui_strings_fuzzer name)
  add_fuzzer(${name}
    SOURCES fuzz_ui_strings.c ${APP_SRC_DIR}/ui/ui_strings.c
            ${UNIT_TESTS_DIR}/strlcpy.c
    INCLUDES ${UNIT_TESTS_DIR}/shims ${APP_SRC_DIR} ${APP_SRC_DIR}/ui
    DEFINITIONS PRINTF= ${ARGN})
  target_compile_options(fuzz_${name} PRIVATE -Wno-unused-value)
endfunction()

add_ui_strings_fuzzer(ui_strings)
add_ui_strings_fuzzer(ui_strings_bagl HAVE_BAGL)
add_ui_strings_fuzzer(ui_strings_nanos TARGET_NANOS)
