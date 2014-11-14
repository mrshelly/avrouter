//
// Copyright (C) 2014 avplayer.org
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>

#include "soci.h"
#include "session.h"
#include "connection-pool.h"

namespace av_router {

	class database
		: public boost::noncopyable
	{
	public:
		explicit database(boost::asio::io_service& io, soci::connection_pool& db_pool);
		~database();

	public:
		typedef boost::function<void(bool result)> result_handler;
		void availability_check(const std::string& user_id, result_handler handler);
		void register_user(const std::string& user_id, const std::string& pubkey, const std::string& email,
			const std::string& telephone, result_handler handler);
		void delete_user(const std::string& user_id, result_handler handler);

	public:
		boost::asio::io_service& m_io_service;
		soci::connection_pool& m_db_pool;
	};


}
