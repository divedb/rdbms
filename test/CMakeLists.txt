function(add_one_test target)
    add_executable(${target} "${target}.cc")
    target_link_libraries(${target} PRIVATE postgres ${GTEST_LIBRARIES})
endfunction()

function(add_tests)
    foreach(target ${ARGV})
        add_one_test(${target})
    endforeach()
endfunction()

add_subdirectory(parser)
add_subdirectory(storage)