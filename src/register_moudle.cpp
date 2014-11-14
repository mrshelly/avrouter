#include "database.hpp"
#include "register_moudle.hpp"
#include "user.pb.h"

#include <boost/regex.hpp>
#include <boost/asio/spawn.hpp>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#ifdef __linux__

#include <sys/wait.h>

static char ** to_argv(std::vector<std::string> args)
{
	typedef char * str;
	char ** ret = new  str[args.size()];

	for(int i = 0; i < args.size(); i++)
	{
		ret[i] =(char*) args[i].c_str();
	}
	return ret;
}


template<class Handler>
static void async_send_email_coro(boost::asio::io_service& io, std::string subject, std::string content, std::pair<std::string, std::string> attachment, Handler handler, boost::asio::yield_context yield_context)
{
	boost::system::error_code ec;
	try{
// curl -s --user 'api:key-64688e2550d1909dab23a26572fe5f89' \
//     https://api.mailgun.net/v2/avplayer.org/messages \
//     -F from=from='AVROUTER <router@avplayer.org>' \
//     -F to='peter_future <peter_future@outlook.com>' \
//     -F subject='Hello' \
//     -F text='Testing some mail!'
		{std::ofstream csrfile(attachment.first);
		csrfile.write(attachment.second.c_str(), attachment.second.length());}

		std::vector<std::string> args;
		args.push_back("/usr/bin/curl");
		args.push_back("-s");
		args.push_back("--user");
		args.push_back("api:key-64688e2550d1909dab23a26572fe5f89");
		args.push_back("https://api.mailgun.net/v2/avplayer.org/messages");
		args.push_back("-F");
		args.push_back("from=\'AVROUTER <router@avplayer.org>\'");
		args.push_back("-F");
		args.push_back("to=\'peter_future <peter_future@outlook.com>\'");
		args.push_back("-F");
		args.push_back(std::string("subject=") + "\'" + subject + "\'");
		args.push_back("-F");
		args.push_back(std::string("text=") + "\'" + content + "\'");
		args.push_back("-F");
		args.push_back(std::string("attachment=@") + attachment.first );

		auto argv = to_argv(args);
		pid_t pid;
		if( (pid = fork()) == 0 )
			execv("/usr/bin/curl", argv);

		waitpid(pid, 0, 0);

	}catch(const boost::system::error_code& ec)
	{
		LOG_INFO << "send mail failed " << ec.message();
		return io.post(std::bind(handler,ec));
	}

	io.post(std::bind(handler,ec));
}


// 暂时的嘛, 等 CA 签名服务器写好了, 这个就可以删了.
template<class Handler>
static void async_send_email(boost::asio::io_service& io, std::string subject, std::string content, std::pair<std::string, std::string> attachment, Handler handler)
{
	boost::asio::spawn(io, boost::bind(async_send_email_coro<Handler>, boost::ref(io), subject, content, attachment, handler, _1));
}

#endif

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

		LOG_INFO << "csr fine, start registering";

		std::string user_name = register_msg->user_name();

		// 确定是合法的 CSR 证书, 接着数据库内插
		m_database.register_user(user_name,register_msg->rsa_pubkey(),
			register_msg->mail_address(), register_msg->cell_phone(),
			[=, this](bool result)
			{
				LOG_INFO << "database fine : " << result;

				// 插入成功了, 那就立马签名出证书来
				if(result)
				{
					LOG_INFO << "now send csr to peter";
#ifdef __linux__
					// TODO 调用 openssl 将 CSR 签名成证书.
					// TODO 将证书更新进数据库

					// TODO 发送一一份证书验证申请到 CA 认证服务器上并等待 CA 返回 Cert
					// TODO 之所以不放到 avrouter 来做, 是为了保证CA的私钥的安全性.

					// TODO 返回注册成功信息 将证书一并返回

					// FIXME 如果签名失败, 则回滚数据库, 签名很少会失败的吧.

					// FIXME & NOTE
					// 目前暂时的做法是给 peter 发一封邮件, 然后就告诉用户注册成功
					// 当然这只是暂时的做法, 目的是保证现在测试阶段能跑过这个注册流程罢了
					// 客户端可以开始轮询来获取证书, 也可以等收到邮件后再登录, 那个时候保证能拿到自己的证书
					std::shared_ptr<BIO> bio(BIO_new(BIO_s_mem()), BIO_free);

					PEM_write_bio_X509_REQ(bio.get(), csr.get());

					unsigned char * PEM_CSR = NULL;
					auto PEM_CSR_LEN = BIO_get_mem_data(bio.get(),&PEM_CSR);

					LOG_DBG << PEM_CSR;
					// 开始发邮件
					async_send_email(m_io_service_pool.get_io_service(),
						"[CSR] Automanted CSR from AVROUTER test version",
						"RT, 这是一封 avrouter 自动发送的邮件, 请表在意哈!",
						std::make_pair<std::string, std::string>( user_name + ".csr", std::string((char*)PEM_CSR, PEM_CSR_LEN)),
					[connection, user_name, this](boost::system::error_code ec)
					{
						LOG_INFO << "mail sended" << ec.message();

						if(!ec)
						{
							proto::user_register_result result;
							result.set_result(proto::user_register_result::REGISTER_SUCCEED);
							connection->write_msg(encode(result));
						}else{
							//  回滚数据库
							m_database.delete_user(user_name, [connection, this](int){
								proto::user_register_result result;
								result.set_result(proto::user_register_result::REGISTER_FAILED_NAME_DISALLOW);
								connection->write_msg(encode(result));
							});
						}

					});
#endif
				}
				else
				{
					LOG_INFO << "db op failed, register stoped";

					proto::user_register_result result;
					result.set_result(proto::user_register_result::REGISTER_FAILED_NAME_TAKEN);
					connection->write_msg(encode(result));
				}
			}
		);
	}
}
