# Resolve the version from git and write version.h.
#
# Run in script mode (cmake -P) from a build-time target rather than at
# configure time, so the version cannot go stale after a commit. configure_file
# leaves the output untouched when the content has not changed, so regenerating
# on every build does not itself cause a rebuild.
#
# Expects: GIT_EXECUTABLE, SRC_DIR, IN_FILE, OUT_FILE.
#
# Output format:
#   v0.0.1                     exactly a tagged release, tree clean
#   v0.0.1+ga46ac93            built past the tag, or from a modified tree
#   v0.0.1+ga46ac93.dirty      uncommitted changes present
#   0.0.0+ga46ac93             no tags reachable at all
#   unknown                    not a git checkout, or git unavailable

set(CBEAM_VERSION "unknown")

if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --long --dirty
        WORKING_DIRECTORY ${SRC_DIR}
        OUTPUT_VARIABLE described
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE described_result
    )

    if(described_result EQUAL 0 AND
       described MATCHES "^(.+)-([0-9]+)-g([0-9a-f]+)(-dirty)?$")
        set(tag      "${CMAKE_MATCH_1}")
        set(distance "${CMAKE_MATCH_2}")
        set(hash     "${CMAKE_MATCH_3}")
        set(dirty    "${CMAKE_MATCH_4}")

        if(distance EQUAL 0 AND NOT dirty)
            # Sitting exactly on the tag with nothing modified: the bare
            # release name is the honest answer, and the hash adds nothing.
            set(CBEAM_VERSION "${tag}")
        else()
            set(CBEAM_VERSION "${tag}+g${hash}")

            if(dirty)
                string(APPEND CBEAM_VERSION ".dirty")
            endif()
        endif()
    else()
        # No tag reachable. Fall back to the bare commit so a binary can still
        # be traced back to a tree.
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --always --dirty
            WORKING_DIRECTORY ${SRC_DIR}
            OUTPUT_VARIABLE fallback
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE fallback_result
        )

        if(fallback_result EQUAL 0 AND fallback)
            string(REPLACE "-dirty" ".dirty" fallback "${fallback}")
            set(CBEAM_VERSION "0.0.0+g${fallback}")
        endif()
    endif()
endif()

get_filename_component(out_dir "${OUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")
configure_file("${IN_FILE}" "${OUT_FILE}" @ONLY)
