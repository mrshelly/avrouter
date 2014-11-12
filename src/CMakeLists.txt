
add_executable(avrouter main.cpp server.cpp io_service_pool.cpp connection.cpp database.cpp root_ca.cpp forward_moudle.cpp login_moudle.cpp)

target_link_libraries(avrouter avimproto)
