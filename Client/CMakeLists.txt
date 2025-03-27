find_package(OpenSSL REQUIRED)

add_executable(Client)

target_sources(Client PRIVATE 
    src/main.cpp
)

target_include_directories(Client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(Client PROPERTIES CXX_STANDARD 20)

target_link_libraries(Client OpenSSL::SSL)

if(WIN32)
    target_link_libraries(Client wsock32.lib ws2_32.lib) 
endif()