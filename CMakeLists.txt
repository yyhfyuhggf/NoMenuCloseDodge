cmake_minimum_required(VERSION 3.21)

project(NoMenuCloseDodge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

# CommonLibSSE-NG ships its own test project; disable it to avoid pulling catch2.
set(BUILD_TESTS OFF)
set(BUILD_TESTS_PROJECT OFF)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG/CMakeLists.txt")
	message(FATAL_ERROR
		"extern/CommonLibSSE-NG is missing.\n"
		"Run build.ps1 (it downloads it), or manually:\n"
		"  git clone --branch ng --depth 1 https://github.com/alandtse/CommonLibSSE-NG.git extern/CommonLibSSE-NG")
endif()

file(GLOB_RECURSE sources ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# CommonLibSSE-NG provides RE:: / SKSE:: APIs and defines add_commonlibsse_plugin().
add_subdirectory(extern/CommonLibSSE-NG)
include(extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake)

add_commonlibsse_plugin(
	${PROJECT_NAME}
	SOURCES
		${sources}
)

find_package(spdlog REQUIRED)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		spdlog::spdlog
)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Source files contain UTF-8 (Chinese comments) - tell MSVC explicitly.
if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
