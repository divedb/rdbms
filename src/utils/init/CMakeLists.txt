add_library(init INTERFACE)
add_library(globals globals.cc)
target_link_libraries(init INTERFACE globals)