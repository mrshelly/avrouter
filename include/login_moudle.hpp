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
		DH * m_dh;
		//  DH 交换算法计算出来的共享密钥
		std::vector<uint8_t> m_shared_key;
	public:
		login_moudle();
		~login_moudle();

	public:
		void process_login_message(google::protobuf::Message*, connection_ptr, connection_manager&);
		void process_hello_message(google::protobuf::Message*, connection_ptr, connection_manager&);
	};

}
