//
// Copyright (C) 2014 avplayer.org
//
// Author: jack
// Email:  jack.wgm at gmail dot com
//

#pragma once

#include <set>
#include <deque>

#include <boost/noncopyable.hpp>
#include <boost/any.hpp>
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

#include <session.h>
#include <connection-pool.h>

#include "logging.hpp"

namespace av_router {

	class database: public boost::noncopyable
	{
	public:
		explicit database(boost::asio::io_service& io, soci::connection_pool & db_pool);
		~database();

	public:
	};


}
