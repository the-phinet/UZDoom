cmake_minimum_required( VERSION 3.16.0 )

project( updater )

if( COMMAND cmake_policy )
	cmake_policy( SET CMP0003 NEW )
endif()

include( CheckCXXSourceCompiles )
include( CheckFunctionExists )
include( CheckCXXCompilerFlag )
include( CheckIncludeFile )
include( CheckIncludeFiles )
include( CheckLibraryExists )
include( FindPkgConfig )

target_architecture(TARGET_ARCHITECTURE)
message(STATUS "Updater architecture is ${TARGET_ARCHITECTURE}")

if ( ${TARGET_ARCHITECTURE} MATCHES "arm64" )
	set (ARM64 aarch64)
endif()

if( ${TARGET_ARCHITECTURE} MATCHES "x86_64" )
	set( X64 64 )
	add_definitions( -DARCH_IA32 )
endif()

if( NOT PROJECT_LIBRARIES )
	set( PROJECT_LIBRARIES "" )
endif()

add_definitions(-DTHIS_IS_UPDATER)

if( WIN32 )

    add_definitions( -D_WIN32 )

    set( PROJECT_LIBRARIES
        ws2_32
        wininet
        user32
        shell32
        advapi32
    )
else()
    if( APPLE )
        add_definitions(-D__FP__ -D__FIXMATH__)
    endif()

    if( UNIX )
        CHECK_LIBRARY_EXISTS(rt clock_gettime "" CLOCK_GETTIME_IN_RT)
        if(CLOCK_GETTIME_IN_RT)
            list(APPEND PROJECT_LIBRARIES rt)
        endif()
    endif()
endif()

if( DEM_CMAKE_COMPILER_IS_GNUCXX_COMPATIBLE )
    set( CMAKE_C_FLAGS "-Wall -Wextra -Wno-unused -Wno-unused-parameter ${CMAKE_C_FLAGS}" )
    set( CMAKE_CXX_FLAGS "-Wall -Wextra -Wno-unused -Wno-unused-parameter ${CMAKE_CXX_FLAGS}" )

    if( WIN32 )
        set( CMAKE_CXX_FLAGS "-D_UNICODE -DUNICODE ${CMAKE_CXX_FLAGS}" )
        set( CMAKE_CXX_FLAGS "-D_WIN32_WINNT=0x0600 ${CMAKE_CXX_FLAGS}" )
    endif()
endif()

if ( CURL_FOUND )
	include_directories( SYSTEM "${CURL_INCLUDE_DIR}" )
	list( APPEND PROJECT_LIBRARIES "${CURL_LIBRARY}" )
    if( MSVC )
        list( APPEND PROJECT_LIBRARIES ${CURL_ROOT_PATH}/lib/zlib.lib )
    endif()
endif()

set( UPDATER_CORE_SOURCES
    update_core.cpp
	update.h
)
add_library( updater_core STATIC ${UPDATER_CORE_SOURCES} )
target_include_directories( updater_core PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/common/thirdparty
)
target_link_libraries( updater_core PRIVATE ${PROJECT_LIBRARIES} miniz )

set( UPDATER_SOURCES
	update.cpp
)
add_executable( updater ${UPDATER_SOURCES} )
set_target_properties( updater PROPERTIES OUTPUT_NAME "update" )
target_include_directories( updater PRIVATE
    ${CMAKE_BINARY_DIR}
)
target_link_libraries( updater
    ${PROJECT_LIBRARIES}
	updater_core
)

if( MSVC )
    target_link_options( updater PRIVATE "/SUBSYSTEM:CONSOLE" )
endif()

if( APPLE )
    set( LINK_FRAMEWORKS "-framework Cocoa -framework Security" )
    set_target_properties( updater PROPERTIES
        LINK_FLAGS "${LINK_FRAMEWORKS}"
    )
endif()

if( DEFINED ZDOOM_OUTPUT_DIR )
    set_target_properties( updater PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${ZDOOM_OUTPUT_DIR}
    )
endif()

if( WIN32 )
    set( INSTALL_PATH . CACHE STRING "Directory where the updater will be placed during install." )
else()
    set( INSTALL_PATH bin CACHE STRING "Directory where the updater will be placed during install." )
endif()

install( TARGETS updater
        DESTINATION ${INSTALL_PATH}
        COMPONENT "Updater")
