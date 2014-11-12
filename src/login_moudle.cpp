#include "database.hpp"
#include "login_moudle.hpp"
#include "user.pb.h"

#include <openssl/dh.h>
#include <openssl/aes.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>

inline std::string RSA_public_decrypt(RSA * rsa, const std::string & from)
{
	std::string result;
	const int keysize = RSA_size(rsa);
	std::vector<unsigned char> block(keysize);

	int inputlen = from.length();

	for(int i = 0 ; i < from.length(); i+= keysize)
	{
		int flen = std::min(keysize, inputlen - i);

		auto resultsize = RSA_public_decrypt(
			flen,
			(uint8_t*) &from[i],
			&block[0],
			rsa,
			RSA_PKCS1_PADDING
		);
		result.append((char*)block.data(), resultsize);
	}
	return result;
}

class cert_validater
	: boost::noncopyable
{
	X509_STORE* m_store;
public:
	cert_validater(X509* ca)
	{
		m_store = X509_STORE_new();

		X509_STORE_add_cert(m_store, ca);
		X509_STORE_set_default_paths(m_store);
	}

	~cert_validater()
	{
		X509_STORE_free(m_store);
	}

	bool verity(X509* cert)
	{
		boost::shared_ptr<X509_STORE_CTX> storeCtx(X509_STORE_CTX_new(), X509_STORE_CTX_free);
		X509_STORE_CTX_init(storeCtx.get(), m_store,cert,NULL);
		X509_STORE_CTX_set_flags(storeCtx.get(), X509_V_FLAG_CB_ISSUER_CHECK);
		return X509_verify_cert(storeCtx.get());
	}

};


namespace av_router {

	login_moudle::login_moudle(av_router::io_service_pool& io_poll, X509* root_ca_cert)
		: m_io_service_pool(io_poll)
		, m_timer(io_poll.get_io_service())
		, m_root_ca_cert(root_ca_cert)
	{
		continue_timer();
	}

	login_moudle::~login_moudle()
	{}

	void login_moudle::process_login_message(google::protobuf::Message* msg, connection_ptr connection, connection_manager&, database&)
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


		unsigned char * CN = NULL;
		auto cert_name = X509_get_subject_name(user_cert.get());
		auto cert_entry = X509_NAME_get_entry(cert_name,
			X509_NAME_get_index_by_NID(cert_name, NID_commonName, 0)
		);
		ASN1_STRING *entryData = X509_NAME_ENTRY_get_data( cert_entry );
		auto strlengh = ASN1_STRING_to_UTF8(&CN, entryData);
		printf("%s\n",CN);
		std::string commonname((char*)CN, strlengh);
		OPENSSL_free(CN);

		// 首先验证用户的证书

		cert_validater cert(m_root_ca_cert);
		bool user_cert_valid = cert.verity(user_cert.get());

		// 证书验证通过后, 用用户的公钥解密 encryped_radom_key 然后比较是否是 login_check_key
		// 如果是, 那么此次就不是冒名登录
		auto evPkey = X509_get_pubkey(user_cert.get());
		auto user_rsa_pubkey = EVP_PKEY_get1_RSA(evPkey);
		EVP_PKEY_free(evPkey);

		auto decrypted_key = RSA_public_decrypt(user_rsa_pubkey, login->encryped_radom_key());
		RSA_free(user_rsa_pubkey);
		bool user_rsa_key_valid = (decrypted_key == login_check_key);

		proto::login_result result;

		// TODO 接着到数据库查询是否阻止登录, 是不是帐号没钱了不给登录了 etc
		if(user_cert_valid && user_rsa_key_valid)
		{
			// 登陆成功.
			login_state& state = iter->second;
			state.status = login_state::succeed;

			result.set_result(proto::login_result::LOGIN_SUCCEED);

			// 记录登录用户名
			connection->store_module_private("user_name", commonname);
		}
		else
		{
			// 登录失败
			result.set_result(proto::login_result::PUBLIC_KEY_MISMATCH);
		}

		std::string response = encode(result);
		connection->write_msg(response);
	}

	void login_moudle::process_hello_message(google::protobuf::Message* hellomsg, connection_ptr connection, connection_manager&, database&)
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
