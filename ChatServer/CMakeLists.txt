find_package(OpenSSL REQUIRED)


add_executable(ChatServer main.cpp)

target_link_libraries(ChatServer OpenSSL::SSL)