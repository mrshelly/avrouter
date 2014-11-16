
add_executable(avrouter main.cpp http_server.cpp avrouterserver.cpp io_service_pool.cpp connection.cpp database.cpp root_ca.cpp forward_moudle.cpp login_moudle.cpp register_moudle.cpp)

target_link_libraries(avrouter avimproto)
