//
// Copyright (C) 2014 avplayer.org
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <session.h>
#include <connection-pool.h>

namespace av_router {

	class database
		: public boost::noncopyable
	{
	public:
		explicit database(boost::asio::io_service& io, soci::connection_pool& db_pool);
		~database();

	public:
		boost::asio::io_service& m_io_service;
		soci::connection_pool& m_db_pool;
	};


}
