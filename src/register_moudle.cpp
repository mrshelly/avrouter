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
	{
		proto::username_availability_check* availabile = dynamic_cast<proto::username_availability_check*>(msg);
		if (!availabile || availabile->user_name().empty())
			return;
		m_database.availability_check(availabile->user_name(),
			[this, connection](bool result)
			{
				// 检查用户名是否可以注册, 如果可以注册, 则返回可以的消息.
				proto::username_availability_result register_result;
				if (result)
				{
					register_result.set_result(proto::username_availability_result::NAME_AVAILABLE);
					LOG_DBG << "register, name available!";
				}
				else
				{
					register_result.set_result(proto::username_availability_result::NAME_TAKEN);
					LOG_DBG << "register, name taken!";
				}
				std::string response = encode(register_result);
				connection->write_msg(response);
			}
		);
	}

	void register_moudle::user_register(google::protobuf::Message* msg, connection_ptr connection, connection_manager&)
	{
		proto::user_register* user_register = dynamic_cast<proto::user_register*>(msg);
		if (!user_register || user_register->user_name().empty())
			return;

		// TODO 检查 CSR 证书是否有伪造

		// TODO 办法, 检查 X509_REQ 签名 X509_REQ_verify() 就可以了

		// 确定是合法的 CSR 证书, 接着数据库内插
		m_database.register_user(user_register->user_name(),user_register->rsa_pubkey(),
			user_register->mail_address(), user_register->cell_phone(),
			[](bool result)
			{
				// 插入成功了, 那就立马签名出证书来
				if(result)
				{
					// TODO 调用 openssl 将 CSR 签名成证书.
					// TODO 将证书更新进数据库

					// TODO 返回注册成功信息 将证书一并返回

					// FIXME 如果签名失败, 则回滚数据库, 签名很少会失败的吧.
				}
			}
		);
	}
}
