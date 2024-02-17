function (AddClangTidy target)
    find_program(CLANG_TIDY_PATH clang-tidy REQUIRED)
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_PATH};-config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")
endfunction()