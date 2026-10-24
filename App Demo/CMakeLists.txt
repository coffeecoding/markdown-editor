cmake_minimum_required(VERSION 3.20)
project(native_markdown LANGUAGES C CXX RC)
string(REPLACE "/O2" "/O1" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
string(REPLACE "/O2" "/O1" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Build with MSVC on Windows (x64 Native Tools prompt).")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This first version targets x64. Select an x64 toolchain.")
endif()

add_executable(markdown-editor WIN32
    main.cpp document.cpp markdown.cpp app.rc
    third_party/md4c/md4c.c third_party/md4c/entity.c)
target_compile_features(markdown-editor PRIVATE cxx_std_17)
target_compile_definitions(markdown-editor PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN
    NOMINMAX _WIN32_WINNT=0x0A00)
target_include_directories(markdown-editor PRIVATE third_party/md4c)
target_link_libraries(markdown-editor PRIVATE comctl32 comdlg32 shell32 gdiplus)
set_property(TARGET markdown-editor PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_compile_options(markdown-editor PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4> /utf-8 $<$<CONFIG:Release>:/GL>)
target_link_options(markdown-editor PRIVATE $<$<CONFIG:Release>:/LTCG>
    $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF> /MANIFEST:NO)

add_custom_command(TARGET markdown-editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/md4c/LICENSE.md"
        "$<TARGET_FILE_DIR:markdown-editor>/MD4C-LICENSE.txt")
