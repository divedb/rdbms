add_library(tcop INTERFACE)
add_library(_postgres postgres.cc)
target_link_libraries(tcop INTERFACE _postgres)