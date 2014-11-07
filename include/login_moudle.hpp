//
// Copyright (C) 2013 Jack.
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include "server.hpp"
#include "serialization.hpp"

namespace av_router {

	class login_moudle
	{
	public:
		login_moudle();
		~login_moudle();

	public:
		void process_message(google::protobuf::Message*, connection_ptr, connection_manager&);
	};

}