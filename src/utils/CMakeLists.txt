add_subdirectory(init)
add_subdirectory(mmgr)

add_library(utils INTERFACE)
target_link_libraries(utils INTERFACE init mmgr)