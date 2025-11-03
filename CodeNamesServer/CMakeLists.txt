cmake_minimum_required(VERSION 3.10)

# vcpkg integration: toolchain must be set BEFORE project() so CMake uses vcpkg during configuration
set(CMAKE_TOOLCHAIN_FILE "G:/공부/C++/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

project(CodeNamesServer LANGUAGES CXX C)

# Export compile_commands.json to help VS Code IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 관련 정의
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
    # suppress Winsock deprecated API warnings to reduce noise
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

# 필요한 패키지 찾기

# 소스 파일 추가
# 소스 파일 추가
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 실행 파일 생성
add_executable(${PROJECT_NAME} ${SOURCES})

# 헤더 파일 경로 (타겟이 생성된 이후에 지정)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Windows 라이브러리 링크
## SQLite 찾기: unofficial-sqlite3 우선, 없으면 일반 sqlite3/SQLite3로 폴백
set(_SQLITE_FOUND FALSE)
find_package(unofficial-sqlite3 CONFIG)
if (unofficial-sqlite3_FOUND)
    set(_SQLITE_FOUND TRUE)
    set(_SQLITE_LIB unofficial::sqlite3::sqlite3)
else()
    # vcpkg나 시스템 패키지로 설치된 일반 sqlite3 패키지 시도
    find_package(SQLite3 CONFIG)
    if (SQLite3_FOUND)
        set(_SQLITE_FOUND TRUE)
        # many SQLite CMake packages export the target SQLite::SQLite3
        set(_SQLITE_LIB SQLite::SQLite3)
    else()
        find_package(sqlite3 CONFIG)
        if (sqlite3_FOUND)
            set(_SQLITE_FOUND TRUE)
            set(_SQLITE_LIB sqlite3::sqlite3)
        endif()
    endif()
endif()

if(WIN32)
    if(_SQLITE_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE 
            ws2_32
            ${_SQLITE_LIB}
        )
    else()
        message(FATAL_ERROR "Could not find sqlite3 (tried unofficial-sqlite3, SQLite3, sqlite3). Please install via vcpkg or set CMAKE_PREFIX_PATH/unofficial-sqlite3_DIR.")
    endif()
endif()

# Make sure Debug builds include full debug info and minimal optimization to make breakpoints reliable
# For multi-config generators (Visual Studio) we add configuration-specific flags via generator expressions.
if(MSVC)
    # MSVC: /Zi for PDB, /Od disable optimization in Debug; /DEBUG linker flag
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Debug>:/Od>
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:/DEBUG>
    )
else()
    # GNU/Clang: -g debug info, -O0 to disable optimizations in Debug-like build type
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-O0>
    )
    # For single-config generators, ensure DEBUG symbols for link step when applicable
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-g>
    )
endif()