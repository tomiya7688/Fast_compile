if(NOT DEFINED FASTCOMPILE OR NOT DEFINED TEST_DIRECTORY)
    message(FATAL_ERROR "FASTCOMPILE and TEST_DIRECTORY are required")
endif()

execute_process(
    COMMAND "${FASTCOMPILE}" "${TEST_DIRECTORY}/missing.fscm" -o "${TEST_DIRECTORY}/missing.o"
    RESULT_VARIABLE result
    ERROR_VARIABLE diagnostic
)

if(result EQUAL 0)
    message(FATAL_ERROR "fastcompile accepted a missing input file")
endif()
if(NOT diagnostic MATCHES "missing.fscm: cannot read source file")
    message(FATAL_ERROR "fastcompile did not explain the missing input: ${diagnostic}")
endif()
