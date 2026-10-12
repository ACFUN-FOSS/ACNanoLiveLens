# FetchContent PATCH_COMMAND helper.
#
# ExternalProject runs PATCH_COMMAND with the working directory set to the
# fetched dependency source dir, so the git commands below operate on metapp.
#
# Required: -DPATCH_FILE=<absolute path to the .patch file>
#
# The patch is applied once, during the download stage. The reverse-apply check
# is only a safety net in case the step is ever run again on an already patched
# tree (it then skips instead of failing).

if(NOT DEFINED PATCH_FILE)
	message(FATAL_ERROR "apply_metapp_patch.cmake: PATCH_FILE is not defined.")
endif()

if(NOT EXISTS "${PATCH_FILE}")
	message(FATAL_ERROR "apply_metapp_patch.cmake: patch file does not exist: ${PATCH_FILE}")
endif()

find_package(Git REQUIRED)

execute_process(
	COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${PATCH_FILE}"
	RESULT_VARIABLE already_applied
	OUTPUT_QUIET
	ERROR_QUIET
)
if(already_applied EQUAL 0)
	message(STATUS "metapp patch already applied, skipping: ${PATCH_FILE}")
	return()
endif()

execute_process(
	COMMAND "${GIT_EXECUTABLE}" apply --whitespace=nowarn "${PATCH_FILE}"
	RESULT_VARIABLE apply_result
	OUTPUT_VARIABLE apply_stdout
	ERROR_VARIABLE apply_stderr
)
if(NOT apply_result EQUAL 0)
	message(FATAL_ERROR
		"Failed to apply metapp patch: ${PATCH_FILE}\n${apply_stdout}${apply_stderr}")
endif()

message(STATUS "Applied metapp patch: ${PATCH_FILE}")
