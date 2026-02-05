cmake_minimum_required(VERSION 3.20)
project(GL_TP LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(nig nigga.cpp src/glad.c)

target_include_directories(nig PRIVATE include)

# target_link_libraries(nig PRIVATE opengl32 glu32 freeglut)
target_link_libraries(nig PRIVATE opengl32 glfw3)
