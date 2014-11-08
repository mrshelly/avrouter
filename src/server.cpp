#include "logging.hpp"
#include "server.hpp"

namespace av_router {

	server::server(io_service_pool& ios, unsigned short port, std::string address /*= "0.0.0.0"*/)
		: m_io_service_pool(ios)
		, m_io_service(ios.get_io_service())
		, m_acceptor(m_io_service)
		, m_timer(m_io_service)
	{
		boost::asio::ip::tcp::resolver resolver(m_io_service);
		std::ostringstream port_string;
		port_string.imbue(std::locale("C"));
		port_string << port;
		boost::system::error_code ignore_ec;
		boost::asio::ip::tcp::resolver::query query(address, port_string.str());
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query, ignore_ec);
		if (ignore_ec)
		{
			LOG_ERR << "Server bind address, DNS resolve failed: " << ignore_ec.message() << ", address: " << address;
			return;
		}
		boost::asio::ip::tcp::endpoint endpoint = *endpoint_iterator;
		m_acceptor.open(endpoint.protocol(), ignore_ec);
		if (ignore_ec)
		{
			LOG_ERR << "Server open protocol failed: " << ignore_ec.message();
			return;
		}
		m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ignore_ec);
		if (ignore_ec)
		{
			LOG_ERR << "Server set option failed: " << ignore_ec.message();
			return;
		}
		m_acceptor.bind(endpoint, ignore_ec);
		if (ignore_ec)
		{
			LOG_ERR << "Server bind failed: " << ignore_ec.message() << ", address: " << address;
			return;
		}
		m_acceptor.listen(boost::asio::socket_base::max_connections, ignore_ec);
		if (ignore_ec)
		{
			LOG_ERR << "Server listen failed: " << ignore_ec.message();
			return;
		}
	}

	server::~server()
	{}

	void server::start()
	{
		start_impl();
		continue_timer();
	}

	void server::start_impl()
	{
		m_connection = boost::make_shared<connection>(boost::ref(m_io_service_pool.get_io_service()), boost::ref(*this), &m_connection_manager);
		m_acceptor.async_accept(m_connection->socket(), boost::bind(&server::handle_accept, this, boost::asio::placeholders::error));
	}

	void server::stop()
	{
		boost::system::error_code ignore_ec;
		m_timer.cancel(ignore_ec);
		m_acceptor.close();
		m_connection_manager.stop_all();
	}

	void server::handle_accept(const boost::system::error_code& error)
	{
		if (!m_acceptor.is_open() || error)
		{
			if (error)
				LOG_ERR << "server::handle_accept, error: " << error.message();
			return;
		}

		m_connection_manager.start(m_connection);

		start_impl();
	}

	void server::on_tick(const boost::system::error_code& error)
	{
		// TODO: 定时要做的事.
		continue_timer();
	}

	void server::do_message(google::protobuf::Message* msg, connection_ptr conn)
	{
		const std::string name = msg->GetTypeName();
		boost::shared_lock<boost::shared_mutex> l(m_message_callback_mtx);
		message_callback_table::iterator iter = m_message_callback.find(name);
		if (iter == m_message_callback.end())
			return;
		iter->second(msg, conn, boost::ref(m_connection_manager)); // 或者直接: m_message_callback[name](msg, conn, boost::ref(m_connection_manager));
	}

	void server::do_connection_notify(int type, connection_ptr conn)
	{
		boost::shared_lock<boost::shared_mutex> l(m_connection_callback_mtx);
		for (const auto& item : m_connection_callback)
			item.second(type, conn, boost::ref(m_connection_manager));
	}

	bool server::add_message_process_moudle(const std::string& name, message_callback cb)
	{
		boost::unique_lock<boost::shared_mutex> l(m_message_callback_mtx);
		if (m_message_callback.find(name) != m_message_callback.end())
		{
			BOOST_ASSERT("module already exist!" && false);
			return false;
		}
		m_message_callback[name] = cb;
		return true;
	}

	bool server::del_message_process_moudle(const std::string& name)
	{
		boost::unique_lock<boost::shared_mutex> l(m_message_callback_mtx);
		if (m_message_callback.find(name) == m_message_callback.end())
		{
			BOOST_ASSERT("not found the moudle" && false);
			return false;
		}
		m_message_callback.erase(name);
		return true;
	}

	bool server::add_connection_process_moudle(const std::string& name, connection_callback cb)
	{
		boost::unique_lock<boost::shared_mutex> l(m_connection_callback_mtx);
		if (m_connection_callback.find(name) != m_connection_callback.end())
		{
			BOOST_ASSERT("module already exist!" && false);
			return false;
		}
		m_connection_callback[name] = cb;
		return true;
	}

	bool server::del_connection_process_moudle(const std::string& name)
	{
		boost::unique_lock<boost::shared_mutex> l(m_connection_callback_mtx);
		if (m_connection_callback.find(name) == m_connection_callback.end())
		{
			BOOST_ASSERT("not found the moudle" && false);
			return false;
		}
		m_connection_callback.erase(name);
		return true;
	}

	void server::continue_timer()
	{
		m_timer.expires_from_now(seconds(1));
		m_timer.async_wait(boost::bind(&server::on_tick, this, boost::asio::placeholders::error));
	}
}
