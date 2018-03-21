cmake_minimum_required(VERSION 2.6)
project(simp)
find_package (Threads)
find_package (X11 REQUIRED)
find_package (OpenGL REQUIRED)

#include_directories()

if (WIN32)
	set (EDITOR_SRCS
	    src/main.cpp
	    src/stb_image.cpp
	    src/os_windows.cpp
	)
else()

	FIND_PACKAGE(PkgConfig REQUIRED)
	PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

	INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
	LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})
	ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

	add_definitions(--std=c++1y -g -fno-rtti)

	set (EDITOR_SRCS
	    src/main.cpp
	    src/stb_image.cpp
	    src/os_linux.cpp
	)

endif()

add_executable(simp ${EDITOR_SRCS})
target_link_libraries(simp ${X11_LIBRARIES} ${OPENGL_gl_LIBRARY} ${CMAKE_THREAD_LIBS_INIT} ${GTK3_LIBRARIES})

