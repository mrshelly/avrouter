//
// Copyright (C) 2013 Jack.
//
// Author: jack
// Email:  jack.wgm@gmail.com
//

#pragma once

#include <set>

#include <boost/noncopyable.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/progress.hpp>
#include <boost/tokenizer.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
using boost::asio::ip::tcp;
#include <boost/date_time.hpp>
using namespace boost::posix_time;

#include "internal.hpp"
#include "http_helper.hpp"
#include "logging.hpp"

namespace av_router {

	class http_connection_manager;

	class http_connection
		: public boost::enable_shared_from_this<http_connection>
		, public boost::noncopyable
	{
	public:
		explicit http_connection(boost::asio::io_service& io, http_connection_manager* connection_man);
		~http_connection();

	public:
		void start();
		void stop();

		tcp::socket& socket();
	private:
		void handle_read_headers(const boost::system::error_code& error, std::size_t bytes_transferred);
		void handle_write_range(const boost::system::error_code& error, std::size_t bytes_transferred);
		void handle_write_http(const boost::system::error_code& error, std::size_t bytes_transferred);

	private:
		boost::asio::io_service& m_io_service;
		tcp::socket m_socket;
		http_connection_manager* m_connection_manager;
		boost::asio::deadline_timer m_try_read_timer;
		boost::asio::streambuf m_request;
		boost::asio::streambuf m_response;
		request_parser m_request_parser;
		request m_http_request;
	};


	typedef boost::shared_ptr<http_connection> http_connection_ptr;
	class http_connection_manager
		: private boost::noncopyable
	{
		enum {
			rate_sec = 5,
		};

		struct statistics {
			int last_rate[rate_sec];
			int rate;
			int cycle;
		};

	public:
		/// Add the specified connection to the manager and start it.
		void start(http_connection_ptr c)
		{
			boost::mutex::scoped_lock l(m_mutex);
			m_connections.insert(c);
			c->start();
		}

		/// Stop the specified connection.
		void stop(http_connection_ptr c)
		{
			boost::mutex::scoped_lock l(m_mutex);
			if (m_connections.find(c) != m_connections.end())
				m_connections.erase(c);
			c->stop();
		}

		/// Stop all connections.
		void stop_all()
		{
			boost::mutex::scoped_lock l(m_mutex);
			std::for_each(m_connections.begin(), m_connections.end(),
				boost::bind(&http_connection::stop, _1));
			m_connections.clear();
		}

		void bytes_transferred(int channel_id, int bytes)
		{
			boost::mutex::scoped_lock l(m_mutex);
			statistics& stat = m_statistics[channel_id];
			stat.last_rate[stat.cycle] += bytes;
		}

		void tick()
		{
			boost::mutex m_mutex;
			std::map<int, statistics>::iterator iter;
			for (iter = m_statistics.begin(); iter != m_statistics.end(); iter++)
			{
				double upload_speed = 0.0f;
				statistics& stat = iter->second;
				for (int i = 0; i < rate_sec; i++)
					upload_speed += stat.last_rate[i];
				upload_speed /= rate_sec;
				if (++stat.cycle > rate_sec)
					stat.cycle = 0;
				stat.last_rate[stat.cycle] = 0;
				stat.rate = upload_speed;
			}
		}

	private:
		boost::mutex m_mutex;
		std::set<http_connection_ptr> m_connections;
		std::map<int, statistics> m_statistics;
	};

}
