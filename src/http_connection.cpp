#include "http_connection.hpp"
#include "escape_string.hpp"
#include "io_service_pool.hpp"
#include "logging.hpp"

namespace av_router {

	http_connection::http_connection(boost::asio::io_service& io, http_connection_manager* connection_man)
		: m_io_service(io)
		, m_socket(io)
		, m_connection_manager(connection_man)
		, m_abort(false)
		, m_try_read_timer(io)
		, m_access_id(-1)
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

		// 重置客户端请求信息.
		m_space_index = m_begin_index = m_end_index = -1;
		m_force_ts = false;

		// 复制http头缓冲区.
		std::vector<char> buffer;
		buffer.resize(m_request.size() + 1);
		buffer[m_request.size()] = 0;
		m_request.sgetn(&buffer[0], m_request.size());

		// 解析HTTP头中的频道name, 检查是否是该频道.
		boost::tribool result;
		m_request_parser.reset();
		m_http_request.headers.clear();
		boost::tie(result, boost::tuples::ignore) = m_request_parser.parse(
			m_http_request, buffer.begin(), buffer.end());

		// 解析请求的频道.
		boost::filesystem::path path(m_http_request.uri);
		std::string channel_id;
		// 如果url中带有force_ts, 也是认为成force_ts.
		// e.g: http://example.com:8080/1221/force_ts
		// 则表示客户端请求的是force_ts格式.
		std::string force_ts = path.leaf().string();
		if (force_ts != "force_ts")
		{
			channel_id = force_ts;
		}
		else
		{
			channel_id = path.branch_path().leaf().string();
			m_force_ts = true;
		}
		m_channel_id = std::atol(channel_id.c_str());
		// 如果频道index为-1, 说明请求的频道是错误的, 则输出错误日志, 并断开.
		if (m_channel_id == -1)
		{
			LOG_ERR << "Request channel error: " << channel_id;
			m_connection_manager->stop(shared_from_this());
			return;
		}

		m_accept_multi = false;
		// 检查是否为force_ts, 以及是否支持多点请求.
		for (std::vector<header>::iterator i = m_http_request.headers.begin();
			i != m_http_request.headers.end(); i++)
		{
			std::string head = boost::trim_copy(i->name);
			std::string value = boost::trim_copy(i->value);

			if (head == "is_force_play" && value == "1")
				m_force_ts = true;

			if (head == "FLT-Space")
			{
				int ret = sscanf(value.c_str(), "%" PRId64 "/%" PRId64 "-%" PRId64, &m_space_index, &m_begin_index, &m_end_index);
				if (ret != 3) m_accept_multi = false;
				LOG_FILE << "FLT-Space: " << value;
			}

			if (head == "FLT-Access")
			{
				m_access_id = std::atol(value.c_str());
				LOG_FILE << "access id: " << value;
			}

			if (head == "User-Agent")
			{
				LOG_FILE << "User-Agent: " << value;
			}
		}
		if (m_space_index != -1 && m_begin_index != -1 && m_end_index != -1)
			m_accept_multi = true;

		// 无论是否是标准http,都将返回最后区间信息.
		boost::int64_t space_index = -1;
		std::ostringstream oss;

		if (space_index == -1)
		{
			LOG_ERR << "no find cache info: " << space_index;
			m_connection_manager->stop(shared_from_this());
			return;
		}
		space_info tmp;
		tmp.space_ = space_index;

		oss << tmp.space_ << "/" << tmp.begin_ << "-" << tmp.end_;
		LOG_DBG << "space info: " << tmp.space_ << ", b: " << tmp.begin_ << ", e: " << tmp.end_;

		// 非多点下载, 则使用最后一个space.
		if (!m_accept_multi)
			m_space_index = space_index;

		LOG_DBG << "response packet num: " << m_packets.size() << ", cache info: " << oss.str();
		m_response.consume(m_response.size());
		std::ostream response_stream(&m_response);
		response_stream << "HTTP/1.1 200 OK\r\n";
		response_stream << "FLT-Cache: " << oss.str() << "\r\n";
		response_stream << "\r\n";
		if (!m_accept_multi)
		{
			// 标准http下载, 从数据尾部的10/1部分开始下载.
			m_begin_index = std::max(tmp.end_ - 1, tmp.begin_);
			m_end_index = tmp.end_; // 在标准http的请求模式下, m_end_index 不起任何作用.
		}
		// 继续读取下一个请求.
		m_request.consume(m_request.size());
		boost::asio::async_read_until(m_socket, m_request, "\r\n\r\n",
			boost::bind(&http_connection::handle_read_headers,
				shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		);
		if (m_accept_multi)
		{
			BOOST_ASSERT(!m_force_ts);
			// 发送http头.
			m_send_count = 0;
			boost::asio::async_write(m_socket, m_response,
				boost::asio::transfer_exactly(m_response.size()),
				boost::bind(&http_connection::handle_write_range,
					shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
		else
		{
			// 直接传输标准http数据, 不作任何处理.
			boost::asio::async_write(m_socket, m_response,
				boost::asio::transfer_exactly(m_response.size()),
				boost::bind(&http_connection::handle_write_http,
					shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
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

		// 统计上行流量.
		m_connection_manager->bytes_transferred(m_channel_id, bytes_transferred);

		// 用于回复force ts流.
		char force_buffer[force_packet_size] = { 0 };
		char *buf = &force_buffer[0];
		const std::size_t max_read_packet = 512;
		std::size_t read_packet = 0;

		// 如果没有读取到数据.
		if (m_response.size() == 0)
		{

		}
		else
		{
			boost::asio::async_write(m_socket, m_response,
				boost::bind(&http_connection::handle_write_http,
					shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
	}

	void http_connection::handle_write_range(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// 出错处理.
		if (error || m_abort)
		{
			m_connection_manager->stop(shared_from_this());
			return;
		}

		// 统计上行流量.
		m_connection_manager->bytes_transferred(m_channel_id, bytes_transferred);

		// 已经发送的数据达到数据尾, 停止发送数据.
		if (m_begin_index >= m_end_index)
			return;

		// 如果频道不存在, 断开.

		// 如果没有发送完成, 则继续发送.
		if (m_packets.size() != 0)
		{
			std::vector<packet_ptr>::iterator iter;
			for (iter = m_packets.begin(); iter != m_packets.end(); iter++)
			{
				packet_ptr& pkt = *iter;
				if (pkt->state == packet::pkt_normal)
					m_response.sputn((const char *)&pkt->data[0], packet_size);
				else
					BOOST_ASSERT("check skip packet!" && false);
				m_begin_index++;
			}
			if (m_response.size() != 0)
			{
				LOG_DBG << "send response: " << m_packets.size() << ", bytes: " << m_response.size();
				boost::asio::async_write(m_socket, m_response,
					boost::bind(&http_connection::handle_write_range,
						shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
			}
			m_packets.clear();
		}
	}

	int http_connection::access_id() const
	{
		return m_access_id;
	}

	int http_connection::channel_id() const
	{
		return m_channel_id;
	}

}
