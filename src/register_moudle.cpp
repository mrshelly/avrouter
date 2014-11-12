#include "database.hpp"
#include "register_moudle.hpp"
#include "user.pb.h"

namespace av_router {

	register_moudle::register_moudle(io_service_pool& io_pool, database& db)
		: m_io_service_pool(io_pool)
		, m_database(db)
	{}

	register_moudle::~register_moudle()
	{}

	void register_moudle::availability_check(google::protobuf::Message* msg, connection_ptr connection, connection_manager&)
	{}

	void register_moudle::user_register(google::protobuf::Message* hellomsg, connection_ptr connection, connection_manager&)
	{}
}
