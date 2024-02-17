add_library(ipc INTERFACE)
add_library(_ipc ipc.cc)
add_library(slock slock.cc)

target_link_libraries(ipc INTERFACE _ipc slock)