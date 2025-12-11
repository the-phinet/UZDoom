#!/usr/bin/cmake -P

# GitInfo.cmake
#
# Public domain. This program uses git commands command to get
# various bits of repository status for a particular directory
# and writes it into a header file so that it can be used for a
# project's versioning.

# Boilerplate to return a variable from a function.
macro(ret_var VAR)
	set(${VAR} "${${VAR}}" PARENT_SCOPE)
endmacro()

# Populate variables "Hash", "Tag", and "Timestamp" with relevant information
# from source repository.  If anything goes wrong return something in "Error."
function(query_repo_info)
	# are we git?
	execute_process(
		COMMAND git rev-parse --is-inside-work-tree
		RESULT_VARIABLE is_git
		OUTPUT_QUIET
	)

	set(Tag "unknown")
	string(TIMESTAMP Timestamp "%Y-%m-%d %H:%M:%S %z")
	set(Hash "0000000")

	if(DEFINED ENV{GIT_DESCRIBE})
		# from env
		set(Tag "$ENV{GIT_DESCRIBE}")

		if (is_git EQUAL "0")
			message(NOTICE "Version tag overridden by GIT_DESCRIBE env var")
		endif()

		# Extract hash from "...-gabcdef"
		string(REGEX MATCH "-g([0-9a-fA-F]+)" match_result "${Tag}")
		if(match_result)
			set(Hash "${CMAKE_MATCH_1}")
		endif()
	elseif(is_git EQUAL "0")
		# from git
		execute_process(
			COMMAND git describe --tags --dirty=-m
			RESULT_VARIABLE Error
			OUTPUT_VARIABLE Temp
			ERROR_QUIET
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		if(NOT "${Error}" STREQUAL "0")
			message(NOTICE "No git tags found! Using fallback '${Tag}'")
		else()
			set(Tag "${Temp}")
		endif()

		execute_process(
			COMMAND git log -1 "--format=%ai;%H"
			RESULT_VARIABLE Error
			OUTPUT_VARIABLE Temp
			ERROR_QUIET
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)

		if(NOT "${Error}" STREQUAL "0")
			message(NOTICE "No git commits found! Using fallback '${Hash}'")
		else()
			string(REPLACE ";" ";" CommitInfo "${Temp}")
			list(GET CommitInfo 0 Timestamp)
			list(GET CommitInfo 1 Hash)
		endif()

	else()
		message(NOTICE "Not a git repo! Set version tag by setting GIT_DESCRIBE env var")
	endif()

	ret_var(Tag)
	ret_var(Timestamp)
	ret_var(Hash)
endfunction()

if(CMAKE_SCRIPT_MODE_FILE AND NOT CMAKE_PARENT_LIST_FILE)
	query_repo_info()
	if(NOT Hash)
		message(FATAL_ERROR "Failed to get commit info: ${Error}")
	endif()
	execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${Tag}")
	execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${Timestamp}")
	execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${Hash}")
endif()
