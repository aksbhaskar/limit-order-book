# Public include directory shared by every target in the project.
add_library(lob_headers INTERFACE)
target_include_directories(lob_headers INTERFACE
    ${CMAKE_SOURCE_DIR}/include)
target_compile_features(lob_headers INTERFACE cxx_std_20)

# Application executable.
add_executable(lob_app main.cpp)
target_link_libraries(lob_app PRIVATE lob_headers lob_warnings)
