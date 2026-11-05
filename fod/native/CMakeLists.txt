cmake_minimum_required(VERSION 3.22)

project(rodin_fp_compat LANGUAGES C CXX)

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

# Runs once at boot; keeps native custom-icon storage owned by its real app UID.
add_executable(rodin_fod_icon_prepare rodin_fod_icon_prepare.c)
target_compile_options(rodin_fod_icon_prepare PRIVATE -Wall -Wextra -Werror)
target_link_options(rodin_fod_icon_prepare PRIVATE -Wl,-z,relro,-z,now)

target_link_libraries(rodin_fod_icon_prepare log)
