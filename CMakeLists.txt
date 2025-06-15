cmake_minimum_required(VERSION 3.15)
project(HL2StyleEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Option A: Use vcpkg for dependencies ===
# (make sure you’ve run: vcpkg install glfw3 assimp)
if(DEFINED ENV{VCPKG_ROOT})
	set(CMAKE_TOOLCHAIN_FILE
		"$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
		CACHE STRING "")
endif()

# === Find dependencies ===
find_package(OpenGL REQUIRED)

# If you didn’t integrate vcpkg toolchain, explicitly point to your vcpkg installs:
set(glfw3_DIR   "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/share/glfw3")
set(assimp_DIR "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/share/assimp")
set(glm_DIR "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows/share/glm")

find_package(glfw3  CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# === Engine source files ===
file(GLOB_RECURSE ENGINE_SOURCES
	 "Engine/Core/*.cpp"    "Engine/Renderer/*.cpp"
	 "Engine/Input/*.cpp"   "Engine/Scene/*.cpp"
	 "Engine/Audio/*.cpp"   "Engine/Physics/*.cpp"
	 "Engine/Scripting/*.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS
	 "Engine/Core/*.h"      "Engine/Renderer/*.h"
	 "Engine/Input/*.h"     "Engine/Scene/*.h"
	 "Engine/Audio/*.h"     "Engine/Physics/*.h"
	 "Engine/Scripting/*.h"
)

add_executable(HL2Engine
	${ENGINE_SOURCES}
	${ENGINE_HEADERS}
	Engine/main.cpp
)

# === Include paths ===
target_include_directories(HL2Engine PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/Engine
	${OPENGL_INCLUDE_DIR}
)

# === Link libraries ===
target_link_libraries(HL2Engine PRIVATE
	glfw
	${OPENGL_gl_LIBRARY}
	assimp::assimp
	# DirectX9:
	d3d9.lib
	glm::glm
)
