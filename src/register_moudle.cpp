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
		proto::user_register* register_msg = dynamic_cast<proto::user_register*>(msg);
		if (!register_msg || register_msg->user_name().empty())
			return;

		// TODO 检查 CSR 证书是否有伪造
		auto in = (const unsigned char *) register_msg->csr().data();

		std::shared_ptr<X509_REQ> csr(d2i_X509_REQ(NULL, &in, register_msg->csr().length()), X509_REQ_free);

		in = (const unsigned char *) register_msg->rsa_pubkey().data();
		std::shared_ptr<RSA> user_rsa_pubkey(d2i_RSAPublicKey(NULL, &in, register_msg->rsa_pubkey().length()), RSA_free);
		std::shared_ptr<EVP_PKEY> user_EVP_PKEY_pubkey(EVP_PKEY_new(), EVP_PKEY_free);
		EVP_PKEY_set1_RSA(user_EVP_PKEY_pubkey.get(), user_rsa_pubkey.get());

		if (X509_REQ_verify(csr.get(), user_EVP_PKEY_pubkey.get()) <= 0)
		{
			// 失败了.

		}

		// 确定是合法的 CSR 证书, 接着数据库内插
		m_database.register_user(register_msg->user_name(),register_msg->rsa_pubkey(),
			register_msg->mail_address(), register_msg->cell_phone(),
			[](bool result)
			{
				// 插入成功了, 那就立马签名出证书来
				if(result)
				{
					// TODO 调用 openssl 将 CSR 签名成证书.
					// TODO 将证书更新进数据库

					// TODO 发送一一份证书验证申请到 CA 认证服务器上并等待 CA 返回 Cert
					// TODO 之所以不放到 avrouter 来做, 是为了保证CA的私钥的安全性.

					// TODO 返回注册成功信息 将证书一并返回

					// FIXME 如果签名失败, 则回滚数据库, 签名很少会失败的吧.
				}
			}
		);
	}
}
