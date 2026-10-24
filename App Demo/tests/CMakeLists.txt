cmake_minimum_required(VERSION 3.20)
project(markdown_mapping_checks LANGUAGES C CXX)
string(REPLACE "/O2" "/O1" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

# Separate, opt-in native checks; nothing is added to the shipped application.
add_executable(mapping-checks mapping.cpp ../document.cpp
    ../third_party/md4c/md4c.c ../third_party/md4c/entity.c)
target_compile_features(mapping-checks PRIVATE cxx_std_17)
target_compile_definitions(mapping-checks PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN
    NOMINMAX _WIN32_WINNT=0x0A00)
target_include_directories(mapping-checks PRIVATE ../third_party/md4c)
set_property(TARGET mapping-checks PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_options(mapping-checks PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4> /utf-8 $<$<CONFIG:Release>:/GL>)
target_link_options(mapping-checks PRIVATE $<$<CONFIG:Release>:/LTCG>
    $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
enable_testing()
add_test(NAME mapping-checks COMMAND mapping-checks)
