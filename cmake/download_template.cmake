function(_FIGMA_DOWNLOAD_LOG MESSAGE_TEXT)
    if(NOT FIGMA_DOWNLOADS_SILENT)
        message(STATUS "${MESSAGE_TEXT}")
    endif()
endfunction()

function(GIT_CLONE NAME URL)
    set(TAG "")
    if(ARGC GREATER 2)
        set(TAG "${ARGV2}")
    endif()

    set(TARGET_DIR "${THIRDPARTY_DIR}/${NAME}")
    if(EXISTS "${TARGET_DIR}/.git")
        _FIGMA_DOWNLOAD_LOG("${NAME}: already cloned")
        if(NOT "${TAG}" STREQUAL "")
            execute_process(
                COMMAND git -C "${TARGET_DIR}" checkout --quiet "${TAG}"
                RESULT_VARIABLE CHECKOUT_RESULT
            )
            if(NOT CHECKOUT_RESULT EQUAL 0)
                message(FATAL_ERROR "${NAME}: failed to checkout ${TAG}")
            endif()
        endif()
        return()
    endif()

    if(EXISTS "${TARGET_DIR}/CMakeLists.txt")
        _FIGMA_DOWNLOAD_LOG("${NAME}: local source exists")
        return()
    endif()

    file(MAKE_DIRECTORY "${THIRDPARTY_DIR}")
    _FIGMA_DOWNLOAD_LOG("${NAME}: cloning ${URL}")
    execute_process(
        COMMAND git clone "${URL}" "${TARGET_DIR}"
        RESULT_VARIABLE CLONE_RESULT
    )
    if(NOT CLONE_RESULT EQUAL 0)
        message(FATAL_ERROR "${NAME}: git clone failed")
    endif()

    if(NOT "${TAG}" STREQUAL "")
        execute_process(
            COMMAND git -C "${TARGET_DIR}" checkout --quiet "${TAG}"
            RESULT_VARIABLE CHECKOUT_RESULT
        )
        if(NOT CHECKOUT_RESULT EQUAL 0)
            message(FATAL_ERROR "${NAME}: failed to checkout ${TAG}")
        endif()
    endif()
endfunction()

