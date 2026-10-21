cmake_minimum_required(VERSION 3.21)

# Baked into Version.h and the SKSE plugin record. A cache variable so a release build can be
# stamped with -DSPA_VERSION=x.y.z without editing this file.
set(SPA_VERSION "1.0.0" CACHE STRING "Version baked into the plugin binary")

project(SKSEPowerAttack VERSION ${SPA_VERSION} LANGUAGES CXX)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Version.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h"
	@ONLY
)

# ---------------------------------------------------------------------------
# CommonLibSSE-NG (alandtse fork) - one binary for SE + AE + VR, not compiled here.
# The checkout is pinned to a release tag and links the prebuilt library that tag publishes
# (COMMONLIB_PREBUILT in the preset, or COMMONLIB_PREBUILT_DIR=<bundle> to reuse an extracted
# one from another project). Any mismatch falls back to a ~16 minute source build.
# ---------------------------------------------------------------------------
set(COMMONLIBSSE_TAG v13.0.0)
set(COMMONLIBSSE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE")
if(NOT EXISTS "${COMMONLIBSSE_DIR}/CMakeLists.txt")
	find_package(Git REQUIRED)
	message(STATUS "extern/CommonLibSSE not found - cloning alandtse/CommonLibSSE-NG at ${COMMONLIBSSE_TAG}...")
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" clone --branch ${COMMONLIBSSE_TAG} --depth 1 --recurse-submodules --shallow-submodules https://github.com/alandtse/CommonLibSSE-NG "${COMMONLIBSSE_DIR}"
		RESULT_VARIABLE _clsse_clone_result
	)
	if(NOT _clsse_clone_result EQUAL 0)
		message(FATAL_ERROR "Failed to clone CommonLibSSE-NG automatically.")
	endif()
endif()

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${COMMONLIBSSE_DIR}" CommonLibSSE)
include("${COMMONLIBSSE_DIR}/cmake/CommonLibSSE.cmake")

# ---------------------------------------------------------------------------
# vcpkg dependencies (directxtk/rapidcsv are required by CommonLibSSE's own find_package)
# ---------------------------------------------------------------------------
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(SOURCES
	src/PCH.h
	src/Plugin.cpp
	src/Version.h.in
	src/Log.h
	src/Config.h
	src/Config.cpp
	src/Chord.h
	src/Binds.h
	src/Selection.h
	src/Attack.h
	src/Attack.cpp
	src/Input.h
	src/Input.cpp
	src/UI.h
	src/UI.cpp
	extern/SKSEMenuFramework.h
)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES})

add_commonlibsse_plugin(${PROJECT_NAME}
	NAME "SKSEPowerAttack"
	AUTHOR "SKSEPowerAttack"
	VERSION "${PROJECT_VERSION}"
	USE_ADDRESS_LIBRARY
	SOURCES ${SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
	src
	extern
	"${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
	CommonLibSSE::CommonLibSSE
	spdlog::spdlog
	nlohmann_json::nlohmann_json
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	PREFIX ""
	OUTPUT_NAME "${PROJECT_NAME}"
)

# ---------------------------------------------------------------------------
# Unit tests for the pure logic (Chord.h, Binds.h, Selection.h). Engine-free on purpose: no
# CommonLibSSE, no SKSE, no precompiled header, so they build in seconds and run without the game.
#   cmake --build build/vs2022 --target spa_tests
#   ctest --test-dir build/vs2022 --output-on-failure
# ---------------------------------------------------------------------------
option(SPA_BUILD_TESTS "Build the unit tests for the pure selection logic" ON)
if(SPA_BUILD_TESTS)
	enable_testing()
	foreach(test_name IN ITEMS SelectionTests BindsTests ChordTests)
		add_executable(spa_${test_name} tests/${test_name}.cpp)
		target_include_directories(spa_${test_name} PRIVATE src)
		target_compile_features(spa_${test_name} PRIVATE cxx_std_23)
		if(MSVC)
			target_compile_options(spa_${test_name} PRIVATE /W4 /permissive- /EHsc /utf-8)
		endif()
		add_test(NAME ${test_name} COMMAND spa_${test_name})
	endforeach()
endif()

# Optional auto-deploy: set MOD_OUTPUT_FOLDER (e.g. in a git-ignored CMakeUserPresets.json) to a
# mod folder. The JSON is not copied: it holds the user's settings.
if(MOD_OUTPUT_FOLDER)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${MOD_OUTPUT_FOLDER}/SKSE/Plugins"
		COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${MOD_OUTPUT_FOLDER}/SKSE/Plugins/"
		VERBATIM
	)
endif()
