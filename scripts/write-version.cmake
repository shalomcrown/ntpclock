if(NOT DEFINED BASE OR NOT DEFINED SRC OR NOT DEFINED OUT)
    message(FATAL_ERROR "write-version.cmake needs BASE, SRC, and OUT")
endif()

# The full version is <project version>.<UTC build time YYYYMMDDHHmm>.<git hash>.
string(TIMESTAMP _stamp "%Y%m%d%H%M" UTC)
set(_hash "nogit")
if(GIT)
    execute_process(
        COMMAND "${GIT}" rev-parse --short HEAD
        WORKING_DIRECTORY "${SRC}"
        OUTPUT_VARIABLE _git_hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _git_result
        ERROR_QUIET
    )
    if(_git_result EQUAL 0 AND _git_hash)
        set(_hash "${_git_hash}")
    endif()
endif()

set(_text "#pragma once\n\n#define NTPCLOCK_VERSION \"${BASE}.${_stamp}.${_hash}\"\n")
set(_old "")
if(EXISTS "${OUT}")
    file(READ "${OUT}" _old)
endif()
# Rewrite only when the text changes, so an unchanged version does not rebuild main.cpp.
if(NOT _old STREQUAL _text)
    file(WRITE "${OUT}" "${_text}")
endif()
