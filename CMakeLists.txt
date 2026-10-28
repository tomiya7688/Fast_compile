cmake_minimum_required(VERSION 3.20)
project(
  fastcompile
  VERSION 0.1.0
  LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_library(fastcompile_core src/compiler.c src/elf_writer.c src/ir.c
                             src/lexer.c src/parser.c src/source_file.c)
target_include_directories(fastcompile_core PUBLIC src)

add_executable(fastcompile src/main.c)
target_link_libraries(fastcompile PRIVATE fastcompile_core)

include(CTest)
if(BUILD_TESTING)
  foreach(golden empty main-void main-int multiple-functions invalid-character)
    configure_file(tests/golden/${golden}.fscm ${golden}.fscm COPYONLY)
  endforeach()
  add_executable(fastcompile_tests tests/compiler_tests.c)
  target_link_libraries(fastcompile_tests PRIVATE fastcompile_core)
  add_test(NAME fastcompile_unit_and_golden COMMAND fastcompile_tests)
  add_test(NAME fastcompile_cli_golden
           COMMAND fastcompile ${CMAKE_CURRENT_BINARY_DIR}/main-int.fscm -o
                   ${CMAKE_CURRENT_BINARY_DIR}/cli-main.o)
  add_test(
    NAME fastcompile_cli_diagnostics
    COMMAND
      ${CMAKE_COMMAND} -DFASTCOMPILE=$<TARGET_FILE:fastcompile>
      -DTEST_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR} -P
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cli_error.cmake)
endif()
