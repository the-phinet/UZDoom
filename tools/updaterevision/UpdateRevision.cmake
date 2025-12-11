#!/usr/bin/cmake -P

# UpdateRevision.cmake
#
# Public domain. This program uses git commands command to get
# various bits of repository status for a particular directory
# and writes it into a header file so that it can be used for a
# project's versioning.

function(main)
	get_filename_component(ScriptDir "${CMAKE_SCRIPT_MODE_FILE}" DIRECTORY)
	get_filename_component(ProjectDir "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)

	include("${ScriptDir}/GitInfo.cmake")

	query_repo_info()
	if(NOT Hash)
		message(FATAL_ERROR "Failed to get commit info: ${Error}")
	endif()


	if (NOT CMAKE_ARGC EQUAL 4) # cmake -P UpdateRevision.cmake <OutputFile>
		message("Usage: ${CMAKE_ARGV2} <path to gitinfo.h>")
		message("Usage: ${CMAKE_ARGV2} --")
		return()
	endif()

	set(OutputFile "${CMAKE_ARGV3}")

	configure_file("${ScriptDir}/gitinfo.h.in" "${OutputFile}" @ONLY)

	file(RELATIVE_PATH RelativeFile "${ProjectDir}" "${OutputFile}")
	message(STATUS "Revision ${RelativeFile}: ${Tag}")
	message(${ScriptDir})
	message(${ProjectDir})
	message(${OutputFile})
	message(${RelativeFile})
endfunction()

main()
