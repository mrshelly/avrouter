#include "http_connection.hpp"
#include "escape_string.hpp"
#include "io_service_pool.hpp"
#include "logging.hpp"
#include "http_server.hpp"

namespace av_router {

	http_connection::http_connection(boost::asio::io_service& io, http_server& serv, http_connection_manager* connection_man)
		: m_io_service(io)
		, m_server(serv)
		, m_socket(io)
		, m_connection_manager(connection_man)
		, m_abort(false)
		, m_try_read_timer(io)
	{}

	http_connection::~http_connection()
	{
		LOG_DBG << "destruct http connection!";
	}

	void http_connection::start()
	{
		m_request.consume(m_request.size());
		m_abort = false;

		boost::system::error_code ignore_ec;
		m_socket.set_option(tcp::no_delay(true), ignore_ec);
		if (ignore_ec)
			LOG_ERR << "http_connection::start, Set option to nodelay, error message :" << ignore_ec.message();

		boost::asio::async_read_until(m_socket, m_request, "\r\n\r\n",
			boost::bind(&http_connection::handle_read_headers,
				shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		);
	}

	void http_connection::stop()
	{
		boost::system::error_code ignore_ec;
		m_abort = true;
		m_socket.close(ignore_ec);
		m_try_read_timer.cancel(ignore_ec);
	}

	tcp::socket& http_connection::socket()
	{
		return m_socket;
	}

	void http_connection::handle_read_headers(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// 出错处理.
		if (error || m_abort)
		{
			m_connection_manager->stop(shared_from_this());
			return;
		}

		// 复制http头缓冲区.
		std::vector<char> buffer;
		buffer.resize(m_request.size() + 1);
		buffer[m_request.size()] = 0;
		m_request.sgetn(&buffer[0], m_request.size());


		m_request_parser.parse(m_http_request, buffer.begin(), buffer.end());

		m_server.handle_request(m_http_request, shared_from_this());

		// 继续读取下一个请求.
		m_request.consume(m_request.size());
		boost::asio::async_read_until(m_socket, m_request, "\r\n\r\n",
			boost::bind(&http_connection::handle_read_headers,
				shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		);
	}

	void http_connection::handle_write_http(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// 出错处理.
		if (error || m_abort)
		{
			m_connection_manager->stop(shared_from_this());
			return;
		}

		BOOST_ASSERT(m_response.size() == 0);

		boost::asio::async_write(m_socket, m_response,
			boost::bind(&http_connection::handle_write_http,
				shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		);
	}
}
