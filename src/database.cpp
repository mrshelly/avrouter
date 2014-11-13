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
						"(user_id text NOT NULL,"	// 用户id, 必填.
						"mail text,"				// mail, 可选.
						"phone text,"				// 电话, 可选.
						"cert bytea,"					// 证书信息, 可选.
						"public_key bytea NOT NULL,"	// 公钥信息, 必填.
						"private_key bytea,"		// 私钥, 可选.
						"allow boolean,"			// 是否允许登陆.
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

	void database::availability_check(const std::string& user_id, result_handler handler)
	{
		std::async(std::launch::async,
			[this, user_id, handler]()
			{
				// 在这里检查数据库中是否存在这个用户名, 检查到的话, 调用对应的handler.
				std::string user;
				soci::session& ses = m_db_pool.at(m_db_pool.lease());
				try
				{
					ses << "SELECT user_id FROM avim_user where user_id = :name", soci::use(user_id), soci::into(user);
				}
				catch (soci::soci_error const & err)
				{
					LOG_ERR << err.what();
					m_io_service.post(boost::bind(handler, false));
					return;
				}
				if (!user.empty())
				{
					m_io_service.post(boost::bind(handler, true));
					return;
				}
				m_io_service.post(boost::bind(handler, false));
			}
		);
	}

}
