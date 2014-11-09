#include <iostream>

#include "io_service_pool.hpp"
#include "login_moudle.hpp"
#include "packet_forward.hpp"
#include "server.hpp"

// 测试数据库.
#include "soci.h"
#include "soci-config.h"
#include "soci-postgresql.h"

using namespace av_router;

void terminator(io_service_pool& ios, server& serv, login_moudle& login)
{
	login.quit();
	serv.stop();
	ios.stop();
}

void test_soci(const soci::backend_factory& backend, std::string conn_str)
{
	soci::session sql(backend, conn_str);
}

int main(int argc, char** argv)
{
	{
		std::string conn_str = "hostaddr = '127.0.0.1' port = '4321' dbname = 'avim' user = 'postgres' password = 'xyz' connect_timeout = '3'";
		soci::backend_factory const &backEnd = *soci::factory_postgresql();

		test_soci(backEnd, conn_str);
	}

	// 8线程并发.
	io_service_pool io_pool(8);

	// 创建服务器.
	server serv(io_pool, 24950);
	// 创建登陆处理模块.
	login_moudle moudle_login(io_pool.get_io_service());
	packet_forward forward_packet(io_pool.get_io_service());

	// 添加登陆处理模块.
	serv.add_message_process_moudle("proto.client_hello", boost::bind(&login_moudle::process_hello_message, &moudle_login, _1, _2, _3));
	serv.add_message_process_moudle("proto.login", boost::bind(&login_moudle::process_login_message, &moudle_login, _1, _2, _3));

	// 添加包的转发处理模块
	serv.add_message_process_moudle("proto.avPacket", boost::bind(&packet_forward::process_packet, &forward_packet, _1, _2, _3));
	serv.add_connection_process_moudle("proto.avPacket", boost::bind(&packet_forward::connection_notify, &forward_packet, _1, _2, _3));
	// 启动服务器.
	serv.start();

	//  Ctrl+c异步处理退出.
	boost::asio::signal_set terminator_signal(io_pool.get_io_service());
	terminator_signal.add(SIGINT);
	terminator_signal.add(SIGTERM);
#if defined(SIGQUIT)
	terminator_signal.add(SIGQUIT);
#endif // defined(SIGQUIT)
	terminator_signal.async_wait(boost::bind(&terminator, boost::ref(io_pool), boost::ref(serv), boost::ref(moudle_login)));

	// 开始启动整个系统事件循环.
	io_pool.run();
	return 0;
}
