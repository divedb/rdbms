add_library(mmgr INTERFACE)
add_library(alloc alloc.cc)
target_link_libraries(mmgr INTERFACE alloc)