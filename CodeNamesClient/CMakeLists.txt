cmake_minimum_required(VERSION 3.10)
project(CodeNamesClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 관련 정의
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

# 헤더 파일 경로
include_directories(${PROJECT_SOURCE_DIR}/include)

# 소스 파일 추가
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 실행 파일 생성
add_executable(${PROJECT_NAME} ${SOURCES})

# Windows 라이브러리 링크
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()