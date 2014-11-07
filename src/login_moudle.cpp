#include "login_moudle.hpp"
#include "user.pb.h"

#include <openssl/dh.h>
#include <openssl/aes.h>

namespace av_router {

	login_moudle::login_moudle()
	{
		m_dh = DH_new();
	}

	login_moudle::~login_moudle()
	{
		DH_free(m_dh);
	}

	void login_moudle::process_login_message(google::protobuf::Message* msg, connection_ptr connection, connection_manager&)
	{
		proto::login * login = dynamic_cast<proto::login*>(msg);

		// TODO 解密 encryped_radom_key 后应该是一个 sha1 hash过的密码
		login->encryped_radom_key();
		// TODO: 处理登陆.login

		// NOTE: m_shared_key 是共享的加密密钥
	}

	void login_moudle::process_hello_message(google::protobuf::Message* hellomsg, connection_ptr connection, connection_manager&)
	{
		unsigned char bin_key[512];

		proto::client_hello * client_hello = dynamic_cast<proto::client_hello*>(hellomsg);
		// 生成随机数然后返回 m_dh->p ，让客户端去算共享密钥
		DH_generate_parameters_ex(m_dh,64,DH_GENERATOR_5,NULL);
		m_dh->g =BN_bin2bn((const unsigned char *) client_hello->random_key().data(), client_hello->random_key().length(), m_dh->g);

		proto::server_hello server_hello;

		server_hello.set_servername("avrouter");
		server_hello.set_version(001);
		server_hello.set_random_key((const void*)bin_key, BN_bn2bin(m_dh->p, bin_key));
		DH_generate_key(m_dh);

		m_shared_key.resize(DH_size(m_dh));
		// 密钥就算出来啦！
		DH_compute_key(&m_shared_key[0], m_dh->pub_key, m_dh);

		std::string response = encode(server_hello);
		// 发回消息
		connection->write_msg(response);
	}

}
