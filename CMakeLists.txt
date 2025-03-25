cmake_minimum_required(VERSION 3.15)
project(GLViewer)

# Slightly modified versions of the files from the cpp-starter-project by Jason Turner:
# https://github.com/lefticus/cpp_starter_project/
include("cmake/CompilerWarnings.cmake") # Enable almost all compiler warnings and CMake option to enable -Werror.

# Download vcpkg and use it to install the dependencies unless vcpkg is already installed.
#if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
#	include("cmake/pmm.cmake")
#	pmm(DEBUG VCPKG
#		REVISION c4937039b0704c711dff11ffa729f1c105b20e42
#		REQUIRES glfw3 glew glm ms-gsl imgui nativefiledialog fmt catch2)
#endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

add_library(VolVis "")
set_project_warnings(VolVis)
include(${CMAKE_CURRENT_LIST_DIR}/src/CMakeLists.txt)
target_include_directories(VolVis PUBLIC "${CMAKE_CURRENT_LIST_DIR}/src/")
target_compile_features(VolVis PUBLIC cxx_std_20)
target_link_libraries(VolVis
	PUBLIC
		glm::glm
		imgui::imgui
		unofficial::nativefiledialog::nfd
		Threads::Threads
		Microsoft.GSL::GSL
		fmt::fmt)

add_executable(Viewer "src/main.cpp")
set_project_warnings(Viewer)
target_link_libraries(Viewer
	PRIVATE
		VolVis
		OpenGL::GL
		glfw
		GLEW::GLEW)

# Copy glsl files to build directory
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/viewer_output_vert.glsl" "${CMAKE_CURRENT_BINARY_DIR}/viewer_output_vert.glsl" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/viewer_output_frag.glsl" "${CMAKE_CURRENT_BINARY_DIR}/viewer_output_frag.glsl" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/wireframe_cube_vert.glsl" "${CMAKE_CURRENT_BINARY_DIR}/wireframe_cube_vert.glsl" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/wireframe_cube_frag.glsl" "${CMAKE_CURRENT_BINARY_DIR}/wireframe_cube_frag.glsl" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/volvis_surface_cube_vert.glsl" "${CMAKE_CURRENT_BINARY_DIR}/volvis_surface_cube_vert.glsl" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/volvis_surface_cube_frag.glsl" "${CMAKE_CURRENT_BINARY_DIR}/volvis_surface_cube_frag.glsl" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/gpu_optimization_vert.glsl" "${CMAKE_CURRENT_BINARY_DIR}/gpu_optimization_vert.glsl" COPYONLY)
# volvis GPU setup
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/volvis_colorcube_frag.glsl" "${CMAKE_CURRENT_BINARY_DIR}/volvis_colorcube_frag.glsl" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/volvis_screen_filling_quad_vert.glsl" "${CMAKE_CURRENT_BINARY_DIR}/volvis_screen_filling_quad_vert.glsl" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/volvis_screen_filling_quad_frag.glsl" "${CMAKE_CURRENT_BINARY_DIR}/volvis_screen_filling_quad_frag.glsl" COPYONLY)
# volvis GPU rendermodes
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/volvis_rendermode_mip_frag.glsl" "${CMAKE_CURRENT_BINARY_DIR}/volvis_rendermode_mip_frag.glsl" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/volvis_rendermode_isosurface_frag.glsl" "${CMAKE_CURRENT_BINARY_DIR}/volvis_rendermode_isosurface_frag.glsl" COPYONLY)
configure_file("${CMAKE_CURRENT_LIST_DIR}/shaders/volvis_rendermode_compositing_frag.glsl" "${CMAKE_CURRENT_BINARY_DIR}/volvis_rendermode_compositing_frag.glsl" COPYONLY)

enable_testing()
add_subdirectory("integrity_tests")
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/grading/")
	add_subdirectory("grading")
endif()
