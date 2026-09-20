cmake_minimum_required(VERSION 3.22)

project(rodin_fp_compat LANGUAGES CXX)

add_library(rodin_fp_compat SHARED
    rodin_fp_compat.cpp
)

set_target_properties(rodin_fp_compat PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(rodin_fp_compat
    binder_ndk
    log
    dl
)
