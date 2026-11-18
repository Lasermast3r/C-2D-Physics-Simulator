cmake_minimum_required(VERSION 3.16)
project(BarakatEn)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- SFML SETUP ---
set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SFML-3.0.2/lib/cmake/SFML")
find_package(SFML 3 COMPONENTS Graphics Window System Audio REQUIRED)

# --- BOX2D SETUP ---
include(FetchContent)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d.git
    GIT_TAG        v2.4.1
)
FetchContent_MakeAvailable(box2d)

# --- EXECUTABLE ---
add_executable(BarakatEn 
    source/mainpart.cpp
    source/Physics.cpp
    source/Physics.hpp

)

target_link_libraries(BarakatEn PRIVATE 
    SFML::Graphics SFML::Window SFML::System SFML::Audio
    box2d
)

# --- COPY RUNTIME DLLS ---
add_custom_command(TARGET BarakatEn POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/SFML-3.0.2/bin"
    $<TARGET_FILE_DIR:BarakatEn>
    COMMENT "Copying SFML DLLs..."
)