#include "database.hpp"
#include "logging.hpp"

namespace av_router {

	database::database(boost::asio::io_service& io, soci::connection_pool& db_pool)
		: m_io_service(io)
		, m_db_pool(db_pool)
	{}

	database::~database()
	{}

}
