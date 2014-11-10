#include <iostream>

#include <session.h>
#include <connection-pool.h>
#include <postgresql/soci-postgresql.h>

#include "io_service_pool.hpp"
#include "login_moudle.hpp"
#include "packet_forward.hpp"
#include "server.hpp"


using namespace av_router;

void terminator(io_service_pool& ios, server& serv, login_moudle& login)
{
	login.quit();
	serv.stop();
	ios.stop();
}

const int poolSize = 32;

int main(int argc, char** argv)
{
#ifdef _WIN32
	// Linux 上使用动态加载, 无需链接到 libsoci_postgresql.so
	// Windows 上因为是静态链接, 所以需要注册一下 postgresql 后端
	// 否则回去找没有编译出来的 libsoci_postgresql.dll
	soci::register_factory_postgresql();
#endif
	// 十个数据库并发链接
	soci::connection_pool db_pool(poolSize);

	// 8线程并发.
	io_service_pool io_pool(8);

	// 创建服务器.
	server serv(io_pool, 24950);

	// 创建数据库连接池
	// 居然不支持 c++11 模式的 range for , 我就不吐槽了
	for (size_t i = 0; i != poolSize; ++i)
	{
		soci::session & sql = db_pool.at(i);
		sql.open("postgresql://dbname=avim");
	}

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
