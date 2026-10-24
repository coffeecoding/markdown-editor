cmake_minimum_required(VERSION 3.20)

project(win32_app_demo LANGUAGES CXX)

add_executable(win32_app_demo WIN32 main.cpp)
target_compile_features(win32_app_demo PRIVATE cxx_std_17)
target_compile_definitions(win32_app_demo PRIVATE UNICODE _UNICODE)
target_link_libraries(win32_app_demo PRIVATE dwmapi uxtheme)
