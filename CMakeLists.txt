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
	# CommonLibSSE-NG 自动生成的插件 TU（build/__<PROJECT>Plugin.cpp）只 include 了
	# 少量 REL/*.h，而 REL/Version.h、REL/Module.h、REL/IDDB.h 自身并不会把需要的
	# 标准库头和 RE:: / stl:: 声明带进来（上游自己的工程是靠预编译头 PCH 解决的）。
	# 结果是 MSVC 报几百行：
	#     error C2039: 'uint16_t' / 'array' / 'string_view' / 'mutex' / 'span' ...
	#     error C2653: 'stl' : is not a class or namespace name
	#     error C3688: invalid literal suffix 'sv'
	# 这里生成一个头文件，用 /FI 强制前置包含给本 target 的**所有**源文件
	#（包括自动生成的那个），等价于上游的 PCH。
	# -----------------------------------------------------------------------
	set(_forced_std_header "${CMAKE_CURRENT_BINARY_DIR}/ForcedStdHeaders.h")
	file(WRITE "${_forced_std_header}"
		"#pragma once\n"
		"#include <cstddef>\n"
		"#include <cstdint>\n"
		"#include <cstdlib>\n"
		"#include <cstring>\n"
		"#include <cwchar>\n"
		"#include <cctype>\n"
		"#include <cmath>\n"
		"#include <ctime>\n"
		"#include <cstdio>\n"
		"#include <climits>\n"
		"#include <cfloat>\n"
		"#include <array>\n"
		"#include <vector>\n"
		"#include <string>\n"
		"#include <string_view>\n"
		"#include <compare>\n"
		"#include <optional>\n"
		"#include <variant>\n"
		"#include <any>\n"
		"#include <tuple>\n"
		"#include <utility>\n"
		"#include <memory>\n"
		"#include <functional>\n"
		"#include <algorithm>\n"
		"#include <numeric>\n"
		"#include <iterator>\n"
		"#include <limits>\n"
		"#include <bit>\n"
		"#include <bitset>\n"
		"#include <ranges>\n"
		"#include <concepts>\n"
		"#include <type_traits>\n"
		"#include <stdexcept>\n"
		"#include <exception>\n"
		"#include <system_error>\n"
		"#include <charconv>\n"
		"#include <format>\n"
		"#include <span>\n"
		"#include <atomic>\n"
		"#include <mutex>\n"
		"#include <shared_mutex>\n"
		"#include <thread>\n"
		"#include <chrono>\n"
		"#include <filesystem>\n"
		"#include <fstream>\n"
		"#include <sstream>\n"
		"#include <iostream>\n"
		"#include <iomanip>\n"
		"#include <locale>\n"
		"#include <codecvt>\n"
		"#include <map>\n"
		"#include <unordered_map>\n"
		"#include <set>\n"
		"#include <unordered_set>\n"
		"#include <deque>\n"
		"#include <list>\n"
		"#include <queue>\n"
		"#include <stack>\n"
		"#include <random>\n"
		"#include <regex>\n"
		"#include <version>\n"
		"#include <RE/Skyrim.h>\n"
		"#include <SKSE/SKSE.h>\n"
		"using namespace std::literals;\n")

	# 源文件里有中文注释（UTF-8），显式告诉 MSVC
	target_compile_options(${PROJECT_NAME} PRIVATE "/FI${_forced_std_header}" /utf-8)
endif()
