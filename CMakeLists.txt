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

if(MSVC)
	# -----------------------------------------------------------------------
	# CommonLibSSE-NG 自动生成的插件 TU（__<PROJECT>Plugin.cpp）只 include 了
	# REL/Version.h，而该头文件本身并不 include <cstdint>/<array>/<string_view>
	# 等标准库头（上游自己的工程是靠预编译头把标准库头先塞进去的）。
	# 结果就是 MSVC 报一大串：
	#     error C2039: 'uint16_t': is not a member of 'std'
	#     error C2039: 'array'   : is not a member of 'std'
	# 这里给本 target 的所有源文件（含自动生成的那个）强制前置包含一组标准库头。
	# -----------------------------------------------------------------------
	set(_forced_std_header "${CMAKE_CURRENT_BINARY_DIR}/ForcedStdHeaders.h")
	file(WRITE "${_forced_std_header}"
		"#pragma once\n"
		"#include <cstddef>\n"
		"#include <cstdint>\n"
		"#include <array>\n"
		"#include <string>\n"
		"#include <string_view>\n"
		"#include <compare>\n"
		"#include <stdexcept>\n"
		"#include <functional>\n"
		"#include <algorithm>\n"
		"#include <charconv>\n"
		"#include <type_traits>\n"
		"#include <utility>\n"
		"#include <memory>\n"
		"#include <limits>\n"
		"#include <optional>\n"
		"#include <vector>\n"
		"#include <system_error>\n"
		"#include <cctype>\n"
		"#include <chrono>\n"
		"#include <filesystem>\n"
		"#include <fstream>\n")

	# 源文件里有中文注释（UTF-8），显式告诉 MSVC
	target_compile_options(${PROJECT_NAME} PRIVATE "/FI${_forced_std_header}" /utf-8)
endif()
