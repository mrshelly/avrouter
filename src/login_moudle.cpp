#include "login_moudle.hpp"
#include "user.pb.h"

#include <openssl/dh.h>
#include <openssl/aes.h>
#include <openssl/x509.h>

namespace av_router {

	login_moudle::login_moudle(av_router::io_service_pool& io_poll)
		: m_io_service_pool(io_poll)
		, m_timer(io_poll.get_io_service())
	{
		continue_timer();
	}

	login_moudle::~login_moudle()
	{}

	void login_moudle::process_login_message(google::protobuf::Message* msg, connection_ptr connection, connection_manager&)
	{
		proto::login* login = dynamic_cast<proto::login*>(msg);
		std::map<ptrdiff_t, login_state>::iterator iter = m_log_state.find(reinterpret_cast<ptrdiff_t>(connection.get()));
		if (iter == m_log_state.end())
			return;

		std::string login_check_key = boost::any_cast<std::string>(
			connection->retrive_module_private("login_check_key"));

		const unsigned char * in = (unsigned char *) login->user_cert().data();

		boost::shared_ptr<X509> user_cert(d2i_X509(NULL, &in , login->user_cert().length()), X509_free);
		connection->store_module_private("user_cert", user_cert);

		// TODO 首先验证用户的证书


		// 证书验证通过后, 用用户的公钥解密 encryped_radom_key 然后比较是否是 login_check_key
		// 如果是, 那么此次就不是冒名登录

		// 接着到数据库查询是否阻止登录, 是不是帐号没钱了不给登录了 etc

		login->encryped_radom_key();

		// 登陆成功.
		login_state& state = iter->second;
		state.status = login_state::succeed;

		proto::login_result result;
		result.set_result(proto::login_result::LOGIN_SUCCEED);
		std::string response = encode(result);
		connection->write_msg(response);
	}

	void login_moudle::process_hello_message(google::protobuf::Message* hellomsg, connection_ptr connection, connection_manager&)
	{
		proto::client_hello* client_hello = dynamic_cast<proto::client_hello*>(hellomsg);
		login_state& state = m_log_state[reinterpret_cast<ptrdiff_t>(connection.get())];
		state.status = login_state::hello;

		std::vector<uint8_t> shared_key;
		DH* dh = DH_new();
		unsigned char bin_key[512] = { 0 };

		// 生成随机数然后返回 m_dh->p ，让客户端去算共享密钥.
		DH_generate_parameters_ex(dh, 64, DH_GENERATOR_5, NULL);
		dh->g = BN_bin2bn((const unsigned char *)client_hello->random_g().data(), client_hello->random_g().length(), dh->g);
		dh->p = BN_bin2bn((const unsigned char *)client_hello->random_p().data(), client_hello->random_p().length(), dh->p);

		DH_generate_key(dh);

		proto::server_hello server_hello;
		server_hello.set_servername("avrouter");
		server_hello.set_version(001);
		server_hello.set_random_pub_key((const void*)bin_key, BN_bn2bin(dh->pub_key, bin_key));
		server_hello.set_server_av_address("router@avplayer.org");

		shared_key.resize(DH_size(dh));
		BIGNUM* client_pubkey = BN_bin2bn((const unsigned char *)client_hello->random_pub_key().data(), client_hello->random_pub_key().length(), NULL);
		DH_compute_key(&shared_key[0], client_pubkey, dh);
		BN_free(client_pubkey);
		DH_free(dh);

		std::string key;
		char buf[16] = { 0 };
		for (int i = 0; i < shared_key.size(); ++i)
		{
			sprintf(buf, "%x%x", (shared_key[i] >> 4) & 0xf, shared_key[i] & 0xf);
			key += buf;
		}

		LOG_DBG << "key: " << key;
		std::string response = encode(server_hello);

		connection->store_module_private("symmetickey", shared_key);
		connection->store_module_private("login_check_key", server_hello.random_pub_key());

		// 发回消息.
		connection->write_msg(response);
	}

	void login_moudle::on_tick(const boost::system::error_code& error)
	{
		if (error)
			return;

		continue_timer();
	}

	void login_moudle::continue_timer()
	{
		m_timer.expires_from_now(seconds(1));
		m_timer.async_wait(boost::bind(&login_moudle::on_tick, this, boost::asio::placeholders::error));
	}

	void login_moudle::quit()
	{
		boost::system::error_code ignore_ec;
		m_timer.cancel(ignore_ec);
	}

}
