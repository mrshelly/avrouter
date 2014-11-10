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
		packet_forward(av_router::io_service_pool&);
		~packet_forward();

	public:
		void connection_notify(int type, connection_ptr, connection_manager&);
		void process_packet(google::protobuf::Message*, connection_ptr, connection_manager&);

	private:
		av_router::io_service_pool& m_io_service_poll;
		std::string m_thisdomain;
		typedef std::map<std::string, connection_weak_ptr> routing_table_type;
		routing_table_type m_routing_table;
	};

}
