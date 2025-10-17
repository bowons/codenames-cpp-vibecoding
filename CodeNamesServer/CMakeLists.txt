cmake_minimum_required(VERSION 3.10)
project(CodeNamesServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg integration
set(CMAKE_TOOLCHAIN_FILE "G:/공부/C++/vcpkg/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "")

# Windows 관련 정의
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# 필요한 패키지 찾기
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# 헤더 파일 경로
include_directories(${PROJECT_SOURCE_DIR}/include)

# 소스 파일 추가
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 실행 파일 생성
add_executable(${PROJECT_NAME} ${SOURCES})

# Windows 라이브러리 링크
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        ws2_32
        unofficial::sqlite3::sqlite3
    )
endif()