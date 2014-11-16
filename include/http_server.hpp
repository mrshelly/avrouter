//
// Copyright (C) 2013 Jack.
//
// Author: jack
// Email:  jack.wgm@gmail.com
//

#pragma once

#include "internal.hpp"
#include "io_service_pool.hpp"
#include "http_connection.hpp"

#include <boost/noncopyable.hpp>

namespace av_router {

	class http_connection;
	class http_server
		: public boost::noncopyable
	{
		friend class http_connection;
	public:
		explicit http_server(io_service_pool& ios, unsigned short port, std::string address = "0.0.0.0");
		~http_server();

	public:
		void start();
		void stop();

	private:
		void handle_accept(const boost::system::error_code& error);
		void on_tick(const boost::system::error_code& error);

		// 收到一个 http request 的时候调用
		void handle_request(const request&, http_connection_ptr);

	private:
		io_service_pool& m_io_service_pool;
		boost::asio::io_service& m_io_service;
		boost::asio::ip::tcp::acceptor m_acceptor;
		bool m_listening;
		http_connection_ptr m_connection;
		http_connection_manager m_connection_manager;
		boost::asio::deadline_timer m_timer;
	};

}
