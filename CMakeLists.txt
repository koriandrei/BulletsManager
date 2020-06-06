cmake_minimum_required(VERSION 2.6)
project(BulletsTest)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Use our modified FindSDL2* modules

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${BulletsTest_SOURCE_DIR}/cmake")
# Set an output directory for our binaries

set(BIN_DIR ${BulletsTest_SOURCE_DIR}/bin)

# Bump up warning levels appropriately for clang, gcc & msvc

# Also set debug/optimization flags depending on the build type. IDE users choose this when

# selecting the build mode in their IDE

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -std=c++11")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG} -g")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE} -O2")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
	if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sdl2)

# Look up SDL2 and add the include directory to our include path
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

# Look in the Lesson0 subdirectory to find its CMakeLists.txt so we can build the executable

add_executable(BulletsTest "")

target_include_directories(BulletsTest 
	PUBLIC
		third_party/
	)

target_sources(BulletsTest 
	PRIVATE 
		src/main.cpp 
		src/Common.cpp
		src/BulletManager.cpp 
		src/Graphics.cpp
	PUBLIC
		src/Common.h
		src/BulletManager.h
		src/Graphics.h
	)

target_link_libraries(${PROJECT_NAME} PUBLIC SDL2::SDL2)

get_filename_component(SDL2_LIB_PATH ${SDL2_LIBRARY} DIRECTORY)

add_custom_command(TARGET BulletsTest POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${SDL2_LIB_PATH}/SDL2.dll" "${BIN_DIR}/Debug")

# install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${BIN_DIR})
