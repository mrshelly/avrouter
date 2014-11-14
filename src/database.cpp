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
			[this]()
			{
				soci::session ses(m_db_pool);
				try
				{
					// 检查数据库是否存在, 如果不存在, 则创建数据库.
					ses << "CREATE DATABASE " << db_name;
				}
				catch (soci::soci_error const& err)
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
				catch (soci::soci_error const& err)
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
				soci::indicator user_name_indicator;

				soci::session ses(m_db_pool);
				try
				{
					ses << "SELECT user_id FROM avim_user WHERE user_id = :name", soci::use(user_id), soci::into(user, user_name_indicator);
				}
				catch (soci::soci_error const& err)
				{
					LOG_ERR << err.what();
					m_io_service.post(boost::bind(handler, false));
					return;
				}
				if (user_name_indicator == soci::i_ok)
				{
					m_io_service.post(boost::bind(handler, true));
					return;
				}
				m_io_service.post(boost::bind(handler, false));
			}
		);
	}

	void database::register_user(std::string user_id, std::string pubkey, std::string email, std::string telephone, result_handler handler)
	{
		std::async(std::launch::async,
			[&, this]()
			{
				// 在这里检查数据库中是否存在这个用户名, 检查到的话, 调用对应的handler.
				std::string user;
				soci::indicator user_name_indicator;

				soci::session ses(m_db_pool);
				try
				{
					// 检查名字没占用, 然后插入个新的
					ses << "SELECT user_id FROM avim_user WHERE user_id = :name", soci::use(user_id), soci::into(user, user_name_indicator);
					if (user_name_indicator == soci::i_ok)
					{
						m_io_service.post(boost::bind(handler, false));
						return;
					}

					// 插入数据库
					ses << "INSERT INTO avim_user (user_id, public_key, mail, phone) VALUES (:name, :pubkey , :email , :phone)"
						, soci::use(user_id), soci::use(pubkey), soci::use(email), soci::use(telephone);

					m_io_service.post(boost::bind(handler, true));
					return;
				}
				catch (soci::soci_error const& err)
				{
					LOG_ERR << err.what();
					m_io_service.post(boost::bind(handler, false));
					return;
				}

				m_io_service.post(boost::bind(handler, false));
			}
		);
	}

	void database::delete_user(const std::string& user_id, database::result_handler handler)
	{
		std::async(std::launch::async,
			[&, this]()
			{
				// 在这里检查数据库中是否存在这个用户名, 检查到的话, 调用对应的handler.
				std::string user;
				soci::indicator user_name_indicator;

				soci::session ses(m_db_pool);
				try
				{
					// 检查名字没占用, 然后插入个新的, 必须是个原子操作
					// FIXME, 其实我也不知道这样用行不行, 有懂数据库的么? 出来说一下
					soci::transaction trans(ses);
					ses << "DELETE FROM avim_user WHERE user_id = :name", soci::use(user_id);
					trans.commit();
					m_io_service.post(boost::bind(handler, true));
					return;
				}
				catch (soci::soci_error const& err)
				{
					LOG_ERR << err.what();
					m_io_service.post(boost::bind(handler, false));
					return;
				}

				m_io_service.post(boost::bind(handler, false));
			}
		);
	}

}
