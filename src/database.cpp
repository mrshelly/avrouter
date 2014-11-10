#include "database.hpp"
#include <future>
#include <boost/thread.hpp>
#include "logging.hpp"


namespace av_router {

	static const std::string db_name = "avim";
	static const std::string db_user_table = "avim_user";

	database::database(boost::asio::io_service& io, soci::connection_pool& db_pool)
		: m_io_service(io)
		, m_db_pool(db_pool)
	{
		std::async(std::launch::async,
			[this](){
				soci::session& ses = m_db_pool.at(m_db_pool.lease());
				try
				{
					// 检查数据库是否存在, 如果不存在, 则创建数据库.
					ses << "CREATE DATABASE " << db_name;
				}
				catch (soci::soci_error const & err)
				{
					LOG_ERR << err.what();
				}
				try
				{
					// 在这里创建数据表!
					ses << "CREATE TABLE " << db_user_table <<
						"(user_id text NOT NULL,"	// 用户id
						"mail text NOT NULL,"		// mail
						"phone text,"				// 电话
						"public_key bytea NOT NULL,"// 公钥
						"private_key bytea,"		// 私钥
						"CONSTRAINT avim_user_pkey PRIMARY KEY(user_id)"
						")"
						"WITH("
						"OIDS = FALSE"
						");"
						"ALTER TABLE avim_user "
						"OWNER TO postgres;";
				}
				catch (soci::soci_error const & err)
				{
					LOG_ERR << err.what();
				}
				// ...
			}
		);
	}

	database::~database()
	{}

}
