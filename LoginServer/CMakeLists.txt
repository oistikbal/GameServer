find_package(OpenSSL REQUIRED)

add_executable(LoginServer)

target_sources(LoginServer PRIVATE 
    src/main.cpp
    src/login_server.cpp

    src/login_server.h
)

set_target_properties(LoginServer PROPERTIES CXX_STANDARD 20)

target_include_directories(LoginServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

target_link_libraries(LoginServer OpenSSL::SSL)

if(WIN32)
    target_link_libraries(LoginServer wsock32.lib ws2_32.lib)
    set_property(TARGET LoginServer PROPERTY CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>" )
endif()