#include <iostream>

#include "io_service_pool.hpp"
#include "login_moudle.hpp"
#include "server.hpp"

using namespace av_router;

void terminator(io_service_pool& ios, server& serv)
{
	serv.stop();
	ios.stop();
}

int main(int argc, char** argv)
{
	// 8线程并发.
	io_service_pool io_pool(8);

	// 创建服务器.
	server serv(io_pool, 5432);
	// 创建登陆处理模块.
	login_moudle moudle_login;
	// 添加登陆处理模块.
	serv.add_message_process_moudle("login", boost::bind(&login_moudle::process_message, &moudle_login, _1, _2, _3));

	// 启动服务器.
	serv.start();

	//  Ctrl+c异步处理退出.
	boost::asio::signal_set terminator_signal(io_pool.get_io_service());
	terminator_signal.add(SIGINT);
	terminator_signal.add(SIGTERM);
#if defined(SIGQUIT)
	terminator_signal.add(SIGQUIT);
#endif // defined(SIGQUIT)
	terminator_signal.async_wait(boost::bind(&terminator, boost::ref(io_pool), boost::ref(serv)));

	// 开始启动整个系统事件循环.
	io_pool.run();
	return 0;
}
