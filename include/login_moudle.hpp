//
// Copyright (C) 2013 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include "server.hpp"
#include "serialization.hpp"

struct dh_st;
typedef struct dh_st DH;

namespace av_router {

	class login_moudle
	{
	public:
		login_moudle(boost::asio::io_service& io);
		~login_moudle();

	public:
		void quit();

		void process_login_message(google::protobuf::Message*, connection_ptr, connection_manager&);
		void process_hello_message(google::protobuf::Message*, connection_ptr, connection_manager&);

	private:
		void on_tick(const boost::system::error_code& error);
		void continue_timer();

	private:
		boost::asio::io_service& m_io_service;
		boost::asio::deadline_timer m_timer;
		struct login_state
		{
			enum state {
				hello,
				succeed,
			};
			state status;
		};
		std::map<ptrdiff_t, login_state> m_log_state;
	};

}
