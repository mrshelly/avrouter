#include "server.hpp"
#include "connection.hpp"
#include "logging.hpp"

namespace av_router {

	connection::connection(boost::asio::io_service& io, server& serv, connection_manager* connection_man)
		: m_io_service(io)
		, m_server(serv)
		, m_socket(io)
		, m_connection_manager(connection_man)
		, m_abort(false)
	{}

	connection::~connection()
	{
		LOG_DBG << "destruct connection: " << this;
	}

	void connection::start()
	{
		LOG_DBG << "start the connection: " << this;

		m_request.consume(m_request.size());
		m_abort = false;

		boost::system::error_code ignore_ec;
		m_socket.set_option(tcp::no_delay(true), ignore_ec);
		if (ignore_ec)
			LOG_ERR << "connection::start, Set option to nodelay, error message :" << ignore_ec.message();

		boost::asio::async_read(m_socket, m_request, boost::asio::transfer_exactly(4),
			boost::bind(&connection::handle_read_header,
				shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		);
	}

	void connection::stop()
	{
		boost::system::error_code ignore_ec;
		m_abort = true;
		m_socket.close(ignore_ec);
	}

	tcp::socket& connection::socket()
	{
		return m_socket;
	}

	void connection::handle_read_header(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// 出错处理.
		if (error || m_abort)
		{
			m_connection_manager->stop(shared_from_this());
			return;
		}


	}

	void connection::handle_read_body(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// 出错处理.
		if (error || m_abort)
		{
			m_connection_manager->stop(shared_from_this());
			return;
		}
	}
}
