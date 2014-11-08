
add_executable(avrouter main.cpp server.cpp login_moudle.cpp io_service_pool.cpp connection.cpp packet_forward.cpp)

target_link_libraries(avrouter avimproto)
