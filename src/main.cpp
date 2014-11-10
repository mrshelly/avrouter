#include <iostream>

#include "database.hpp"
#include "io_service_pool.hpp"
#include "login_moudle.hpp"
#include "packet_forward.hpp"
#include "server.hpp"

// 测试数据库.
#include <soci.h>
#include <soci-config.h>
#include <session.h>
#include <connection-pool.h>
#include <postgresql/soci-postgresql.h>

using namespace av_router;

void terminator(io_service_pool& ios, server& serv, login_moudle& login)
{
	login.quit();
	serv.stop();
	ios.stop();
}


const int pool_size = 32;
const std::string connection_string = "hostaddr = '127.0.0.1' "
	"port = '4321' "
	"user = 'postgres' "
	"password = 'avrouter' "
	"dbname = 'avim' "
	"connect_timeout = '3' "
	"application_name = 'avrouter'";

int main(int argc, char** argv)
{
	// 直接指定数据库后端, 避免寻找dll而失败, 这里指定为postgresql数据库后端.
	soci::backend_factory const &db_backend(*soci::factory_postgresql());
	// 十个数据库并发链接.
	soci::connection_pool db_pool(pool_size);
	// 8线程并发.
	io_service_pool io_pool(8);

	// 创建服务器.
	server serv(io_pool, 24950);

	try
	{
		// 创建数据库连接池.
		for (size_t i = 0; i != pool_size; ++i)
		{
			soci::session& sql = db_pool.at(i);
			// 连接本机的数据库.
			sql.open(db_backend, connection_string);
		}
	}
	catch (soci::soci_error& ec)
	{
		LOG_ERR << "create database connection pool failed, error: " << ec.what();
	}

	database async_database(io_pool.get_io_service(), db_pool);

	// 创建登陆处理模块.
	login_moudle moudle_login(io_pool);
	packet_forward forward_packet(io_pool);

	// 添加登陆处理模块.
	serv.add_message_process_moudle("proto.client_hello", boost::bind(&login_moudle::process_hello_message, &moudle_login, _1, _2, _3, boost::ref(async_database)));
	serv.add_message_process_moudle("proto.login", boost::bind(&login_moudle::process_login_message, &moudle_login, _1, _2, _3, boost::ref(async_database)));

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
