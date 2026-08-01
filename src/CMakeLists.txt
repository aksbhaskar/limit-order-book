# Public include directory shared by every target in the project.
add_library(lob_headers INTERFACE)
target_include_directories(lob_headers INTERFACE
    ${CMAKE_SOURCE_DIR}/include)
target_compile_features(lob_headers INTERFACE cxx_std_20)

# Core library: the order model and supporting value types. The matching engine
# and order book will be added to this target in later milestones.
add_library(lob
    order.cpp
)
target_link_libraries(lob PUBLIC lob_headers)
target_link_libraries(lob PRIVATE lob_warnings)

# Application executable.
add_executable(lob_app main.cpp)
target_link_libraries(lob_app PRIVATE lob lob_warnings)
