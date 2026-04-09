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
