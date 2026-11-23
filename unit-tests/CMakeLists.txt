cmake_minimum_required(VERSION 3.10)

project(unit_tests LANGUAGES C)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

include(CTest)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_definitions(-DPRINTF=)
# Compile the swap-only parser state so that swap validation can be tested
add_definitions(-DHAVE_SWAP)
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -pedantic -g -O0 --coverage")
set(GCC_COVERAGE_LINK_FLAGS "--coverage -lgcov")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS}")

set(PARSER_SOURCES
  ../app/src/parser/formatting.c
  ../app/src/parser/parser_state.c
  ../app/src/parser/num_parser.c
  ../app/src/parser/micheline_parser.c
  ../app/src/parser/fa2_tokens.c
  ../app/src/parser/operation_parser.c
)

add_executable(test_operation_parser
  test_operation_parser.c
  strlcpy.c
  digestif/sha256.c
  ${PARSER_SOURCES}
)

target_include_directories(test_operation_parser PRIVATE
  ../app/src/parser
  digestif
)

target_compile_options(test_operation_parser PRIVATE
  -Wall
  -Wextra
  -Wconversion
  -Wredundant-decls
  -Wshadow
  -Wno-unused-parameter
)

target_link_libraries(test_operation_parser PRIVATE
  cmocka
)

add_test(NAME test_operation_parser COMMAND test_operation_parser)

add_executable(test_formatting
  test_formatting.c
  strlcpy.c
  digestif/sha256.c
  ../app/src/parser/formatting.c
)

target_include_directories(test_formatting PRIVATE
  ../app/src/parser
  digestif
)

target_compile_options(test_formatting PRIVATE
  -Wall
  -Wextra
  -Wconversion
  -Wredundant-decls
  -Wshadow
  -Wno-unused-parameter
)

target_link_libraries(test_formatting PRIVATE
  cmocka
)

add_test(NAME test_formatting COMMAND test_formatting)

# ui_strings.c includes SDK headers, so shims/ stands in for them. It must come
# first so that <io.h>, <os.h> and "globals.h" resolve to the host versions.
add_executable(test_ui_strings
  test_ui_strings.c
  strlcpy.c
  ../app/src/ui/ui_strings.c
)

target_include_directories(test_ui_strings PRIVATE
  shims
  ../app/src
  ../app/src/ui
)

# No -Wredundant-decls here: ui_strings.c repeats the prototypes of its header.
target_compile_options(test_ui_strings PRIVATE
  -Wall
  -Wextra
  -Wconversion
  -Wshadow
  -Wno-unused-parameter
)

target_link_libraries(test_ui_strings PRIVATE
  cmocka
)

add_test(NAME test_ui_strings COMMAND test_ui_strings)

# The history of screens of the NBGL review: the application files are linked
# as they are, against the stand-ins of the SDK in shims/nbgl.
add_executable(test_ui_stream_nbgl
  test_ui_stream_nbgl.c
  strlcpy.c
  ../app/src/ui/ui_strings.c
  ../app/src/ui/ui_stream_common.c
  ../app/src/ui/ui_stream_nbgl.c
)

target_include_directories(test_ui_stream_nbgl PRIVATE
  shims/nbgl
  shims
  ../app/src
  ../app/src/ui
  ../app/src/parser
)

target_compile_definitions(test_ui_stream_nbgl PRIVATE HAVE_NBGL)

target_compile_options(test_ui_stream_nbgl PRIVATE
  -Wall
  -Wextra
  -Wno-unused-parameter
)

target_link_libraries(test_ui_stream_nbgl PRIVATE
  cmocka
)

add_test(NAME test_ui_stream_nbgl COMMAND test_ui_stream_nbgl)

add_executable(test_swap_key_policy
  test_swap_key_policy.c
  ../app/src/swap_key_policy.c
)

target_include_directories(test_swap_key_policy PRIVATE
  ../app/src
)

target_compile_options(test_swap_key_policy PRIVATE
  -Wall
  -Wextra
  -Wconversion
  -Wredundant-decls
  -Wshadow
  -Wno-unused-parameter
)

target_link_libraries(test_swap_key_policy PRIVATE
  cmocka
)

add_test(NAME test_swap_key_policy COMMAND test_swap_key_policy)

# Replays the samples committed in tests/samples through the parser. They are
# generated from Octez (see tests/samples/README.md), running them is not.
add_executable(test_parser_samples
  test_parser_samples.c
  strlcpy.c
  digestif/sha256.c
  ${PARSER_SOURCES}
)

target_compile_definitions(test_parser_samples PRIVATE
  SAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../tests/samples"
)

target_include_directories(test_parser_samples PRIVATE
  ../app/src/parser
  digestif
)

target_compile_options(test_parser_samples PRIVATE
  -Wall
  -Wextra
  -Wconversion
  -Wredundant-decls
  -Wshadow
  -Wno-unused-parameter
)

target_link_libraries(test_parser_samples PRIVATE
  cmocka
)

add_test(NAME test_parser_samples COMMAND test_parser_samples)
