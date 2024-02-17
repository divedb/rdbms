add_subdirectory(parser)
add_subdirectory(storage)
add_subdirectory(tcop)
add_subdirectory(utils)

add_library(postgres INTERFACE)
target_link_libraries(postgres INTERFACE parser storage tcop utils)