cmake_minimum_required(VERSION 3.20)

project(TezosFuzzer
        VERSION 1.0
        DESCRIPTION "Ledger Tezos App Fuzzer"
        LANGUAGES C)

if(NOT DEFINED BOLOS_SDK)
  message(FATAL_ERROR "BOLOS_SDK must be defined")
endif()

# Benign wrap-arounds of the app, on top of the ignorelist of the SDK.
set(LEDGER_FUZZ_APP_UBSAN_IGNORELIST ${CMAKE_SOURCE_DIR}/ubsan-ignorelist.txt)

# Exports the `secure_sdk` target and ledger_fuzz_add_app_target().
include(${BOLOS_SDK}/fuzzing/cmake/LedgerAppFuzz.cmake)
ledger_fuzz_setup()

set(APP_SOURCE_DIR ${CMAKE_SOURCE_DIR}/..)

# The whole app plus the fuzz stubs; app_main.c is replaced by the fuzz entry
# point. The glyphs are generated from app/glyphs by the framework: the ones
# that an app build leaves in src/ would be defined twice.
file(GLOB_RECURSE C_SOURCES CONFIGURE_DEPENDS
    ${APP_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/mock/*.c)
list(REMOVE_ITEM C_SOURCES ${APP_SOURCE_DIR}/src/app_main.c)
list(FILTER C_SOURCES EXCLUDE REGEX ".*/src/glyphs\\.c$")

ledger_fuzz_collect_include_dirs(APP_INCLUDE_DIRS ${APP_SOURCE_DIR}/src)
# mock/ has the headers of the harnesses, and checks/ what they check with:
# the checks, their reference, and the driver that feeds a parser in the
# slices the app gets its data in.
list(APPEND APP_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/mock
    ${CMAKE_SOURCE_DIR}/checks)

# HAVE_* / APPNAME / ... arrive transitively from secure_sdk
# (make list-defines + macros/add_macros.txt).
#
# EXCLUDE drops a source of the app, for a harness that includes it, and
# EXTRA and INCLUDES add what only that target needs.
function(tezos_add_fuzz_target NAME)
  cmake_parse_arguments(T "" "" "EXCLUDE;EXTRA;INCLUDES" ${ARGN})

  set(SOURCES ${C_SOURCES})
  if(T_EXCLUDE)
    list(REMOVE_ITEM SOURCES ${T_EXCLUDE})
  endif()

  # Invariant models are machine-local and regenerated per build (.gitignored).
  set(INVARIANT "${CMAKE_SOURCE_DIR}/invariants/${NAME}.zon")
  ledger_fuzz_bootstrap_file("${INVARIANT}" ".{}\n")
  ledger_fuzz_add_app_target(
      NAME ${NAME}
      HARNESS ${CMAKE_SOURCE_DIR}/harness/${NAME}.c
      INVARIANT ${INVARIANT}
      SOURCES ${SOURCES} ${T_EXTRA}
      INCLUDE_DIRECTORIES ${APP_INCLUDE_DIRS} ${T_INCLUDES})
endfunction()

# Full-app target: one input drives a sequence of APDUs through the real
# dispatcher.
tezos_add_fuzz_target(fuzz_app)

# The swap mode, which the callbacks of Exchange enter instead of an INS.
tezos_add_fuzz_target(fuzz_swap)

# The parsers of what is signed, behind the packets of a signing that fuzz_app
# has to get through first.
tezos_add_fuzz_target(fuzz_parser)

# The helpers behind the parsers and the screens, held to what they must
# satisfy. The harness includes operation_parser.c to reach what is static in
# it, in place of the copy of the app.
tezos_add_fuzz_target(fuzz_helpers
    EXCLUDE ${APP_SOURCE_DIR}/src/parser/operation_parser.c)
