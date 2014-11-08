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

	class packet_forward
	{
	public:
		packet_forward(boost::asio::io_service& io);
		~packet_forward();

	public:
		void connection_notify(int type, connection_ptr, connection_manager&);

		void process_packet(google::protobuf::Message*, connection_ptr, connection_manager&);

	private:
		boost::asio::io_service& m_io_service;
		std::string m_thisdomain;

		typedef  std::map<std::string, connection_ptr> routine_table_type;
	};

}
