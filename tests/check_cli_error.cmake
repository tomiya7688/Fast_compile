if(NOT DEFINED FASTCOMPILE OR NOT DEFINED TEST_DIRECTORY)
  message(FATAL_ERROR "FASTCOMPILE and TEST_DIRECTORY are required")
endif()

execute_process(
  COMMAND "${FASTCOMPILE}" "${TEST_DIRECTORY}/missing.fscm" -o
          "${TEST_DIRECTORY}/missing.o"
  RESULT_VARIABLE result
  ERROR_VARIABLE diagnostic)

if(NOT result EQUAL 3)
  message(FATAL_ERROR "expected input error exit code 3, got: ${result}")
endif()
if(NOT diagnostic MATCHES "missing.fscm: cannot read source file")
  message(
    FATAL_ERROR "fastcompile did not explain the missing input: ${diagnostic}")
endif()

set(invalid_source "${TEST_DIRECTORY}/invalid-character.fscm")
execute_process(
  COMMAND "${FASTCOMPILE}" "${invalid_source}" -o
          "${TEST_DIRECTORY}/invalid-character.o"
  RESULT_VARIABLE result
  ERROR_VARIABLE diagnostic)
if(NOT result EQUAL 4
   OR NOT diagnostic MATCHES
      "invalid-character.fscm:1:1: error: invalid source character")
  message(
    FATAL_ERROR "fastcompile did not report the input location: ${diagnostic}")
endif()
