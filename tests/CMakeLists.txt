include_directories("..")
link_directories(${LIBRARY_OUTPUT_PATH})
add_executable(construct construct.cpp)
target_link_libraries(construct jmpool)
add_executable(allocator allocator.cpp)
target_link_libraries(allocator jmpool)
