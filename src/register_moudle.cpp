﻿#include "database.hpp"
#include "register_moudle.hpp"
#include "user.pb.h"
#include "ca.pb.h"

#include <future>
#include <boost/regex.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/format.hpp>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

namespace av_router {

	namespace detail {

		template<class AsyncStream>
		static google::protobuf::Message* async_read_protobuf_message(AsyncStream &s, boost::asio::yield_context yield_context)
		{
			using namespace boost::asio;
			std::string buf;
			buf.resize(4);
			async_read(s, buffer(&buf[0], 4), yield_context);

			uint32_t pktlen = ntohl(*(uint32_t*)(buf.data()));
			if( pktlen >= 10000)
				return nullptr;
			buf.resize(pktlen + 4);

			async_read(s, buffer(&buf[4], pktlen), yield_context);

			return decode(buf);
		}

		// NOTE: 看上去很复杂, 不过这个是暂时的, 因为稍后会用大兽模式, 与 CA 建立长连接来处理
		// 而不是像现在这样使用短连接+轮询
		template<class Handler>
		static void async_send_csr_coro(boost::asio::io_service& io, std::string csr, std::string rsa_fingerprint, Handler handler, boost::asio::yield_context yield_context)
		{
			// 链接到 ca.avplayer.org:8086
			using namespace boost::asio;

			ip::tcp::socket socket(io);

			ip::tcp::resolver resolver(io);

			auto endpoint_it = resolver.async_resolve(ip::tcp::resolver::query("ca.avplayer.org", "8086"), yield_context);

			async_connect(socket, endpoint_it, yield_context);

			// 发送 push_csr 请求

			proto::ca::csr_push csr_push;
			csr_push.set_csr(csr);

			async_write(socket, buffer(encode(csr_push)), yield_context);

			bool push_ready = false;
			// 等待 push_ok
			do{
				std::shared_ptr<google::protobuf::Message> msg(async_read_protobuf_message(socket, yield_context));

				if (msg && msg->GetTypeName() == "proto.ca.push_ok")
				{
					for (const std::string& fingerprint : dynamic_cast<proto::ca::push_ok*>(msg.get())->fingerprints())
					{
						if (fingerprint == rsa_fingerprint)
							push_ready = true;
					}
				}
			}while(false);

			if(!push_ready)
			{
				io.post(std::bind(handler, -1, std::string()));
				return;
			}

			deadline_timer timer(io);

			std::atomic<bool> can_read;
			can_read = false;

			async_read(socket, null_buffers(), [&can_read](boost::system::error_code ec, std::size_t){
				can_read = !ec;
			});

			// 十秒后取消读取
			timer.expires_from_now(boost::posix_time::seconds(10));
			timer.async_wait([&socket](boost::system::error_code ec){
				if(!ec)
					socket.cancel();
			});

			// 每秒轮询一次 pull_cert
			// 只轮询 10 次, 这样要求 10s 内给出结果
			for (int i=0; i < 10 && !can_read; i++)
			{
				proto::ca::cert_pull cert_pull;
				cert_pull.set_fingerprint(rsa_fingerprint);
				async_write(socket, buffer(encode(csr_push)), yield_context);
				deadline_timer timer(io);
				timer.expires_from_now(boost::posix_time::seconds(1));
				timer.async_wait(yield_context);
			}

			timer.cancel();

			if (can_read)
			{
				// 返回 cert
				std::shared_ptr<google::protobuf::Message> msg(async_read_protobuf_message(socket, yield_context));

				if (msg && msg->GetTypeName() == "proto.ca.cert_push" && dynamic_cast<proto::ca::cert_push*>(msg.get())->fingerprint() == rsa_fingerprint)
				{
					io.post(std::bind(handler, 0, dynamic_cast<proto::ca::cert_push*>(msg.get())->cert()));
				}
			}

			io.post(std::bind(handler, 1, std::string()));
		}
	} // namespace detail

	// 暂时的嘛, 等 CA 签名服务器写好了, 这个就可以删了.
	template<class Handler>
	static void async_send_csr(boost::asio::io_service& io, std::string csr, std::string rsa_figureprint, Handler handler)
	{
		// 开协程, 否则编程太麻烦了, 不是么?
		boost::asio::spawn(io, boost::bind(detail::async_send_csr_coro<Handler>, boost::ref(io), csr, rsa_figureprint, handler, _1));
	}


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

	// HTTP 版本, 大同小异, 只是返回的不是 protobuf 消息, 而是 json 格式的消息
	void register_moudle::availability_check_httpd(const request& req, http_connection_ptr conn, http_connection_manager&)
	{
		std::string user_name;
		// TODO 添加实现.
		LOG_DBG << "register_moudle::availability_check_httpd called";

		LOG_DBG << req.body;

		http_form request_parameter(req.body, req["content-type"]);
		user_name = request_parameter["username"];


		m_database.availability_check(user_name, [conn](int result){
			// TODO 返回 json 数据
			auto body = boost::str(boost::format("{\"code\" : \"%d\"}") % result);
			conn->write_response(body);
		});
	}

	void register_moudle::user_register(google::protobuf::Message* msg, connection_ptr connection, connection_manager&)
	{
		proto::user_register* register_msg = dynamic_cast<proto::user_register*>(msg);
		if (!register_msg || register_msg->user_name().empty())
			return;

		// TODO 检查 CSR 证书是否有伪造
		auto in = (const unsigned char *)register_msg->csr().data();

		std::string rsa_pubkey = register_msg->rsa_pubkey();

		std::shared_ptr<X509_REQ> csr(d2i_X509_REQ(NULL, &in, static_cast<long>(register_msg->csr().length())), X509_REQ_free);

		in = (const unsigned char *)rsa_pubkey.data();
		std::shared_ptr<RSA> user_rsa_pubkey(d2i_RSA_PUBKEY(NULL, &in, static_cast<long>(rsa_pubkey.length())), RSA_free);
		std::shared_ptr<EVP_PKEY> user_EVP_PKEY_pubkey(EVP_PKEY_new(), EVP_PKEY_free);
		EVP_PKEY_set1_RSA(user_EVP_PKEY_pubkey.get(), user_rsa_pubkey.get());

		if (X509_REQ_verify(csr.get(), user_EVP_PKEY_pubkey.get()) <= 0)
		{
			// 失败了.
			return proto_write_user_register_response(proto::user_register_result::REGISTER_FAILED_CSR_VERIFY_FAILURE, boost::optional<std::string>(), connection);
		}

		LOG_INFO << "csr fine, start registering";

		// 确定是合法的 CSR 证书, 接着数据库内插
		std::string user_name = register_msg->user_name();
		m_database.register_user(user_name, rsa_pubkey, register_msg->mail_address(), register_msg->cell_phone(),
			[=](bool result)
			{
				LOG_INFO << "database fine : " << result;

				// 插入成功了, 那就立马签名出证书来
				if(result)
				{
					LOG_INFO << "now send csr to peter";

					std::string rsa_figureprint;

					rsa_figureprint.resize(20);

					SHA1((unsigned char*)rsa_pubkey.data(), rsa_pubkey.length(), (unsigned char*) &rsa_figureprint[0]);

					std::shared_ptr<BIO> bio(BIO_new(BIO_s_mem()), BIO_free);
					PEM_write_bio_X509_REQ(bio.get(), csr.get());

					unsigned char* PEM_CSR = NULL;
					auto PEM_CSR_LEN = BIO_get_mem_data(bio.get(), &PEM_CSR);
					std::string pem_csr((char*)PEM_CSR, PEM_CSR_LEN);

					LOG_DBG << pem_csr;

					async_send_csr(m_io_service_pool.get_io_service(), pem_csr, rsa_figureprint,
					[=](int result, std::string cert)
					{
						LOG_INFO << "csr sended";

						if (result == 0)
						{
							// 将 CERT 存入数据库.
							m_database.update_user_cert(user_name, cert, std::bind(&register_moudle::proto_write_user_register_response, this,
								// 向用户返回可以马上登录!
								proto::user_register_result::REGISTER_SUCCEED, boost::optional<std::string>(cert), connection));

						}
						else if (result == 1)
						{
							// 注册成功, CERT 等待.
							proto_write_user_register_response(proto::user_register_result::REGISTER_SUCCEED_PENDDING_CERT, boost::optional<std::string>(), connection);
						}
						else
						{
							//  回滚数据库.
							m_database.delete_user(user_name,
								std::bind(&register_moudle::proto_write_user_register_response, this,
									proto::user_register_result::REGISTER_FAILED_CA_DOWN, boost::optional<std::string>(), connection));
						}
					});
				}
				else
				{
					LOG_INFO << "db op failed, register stoped";

					proto_write_user_register_response(proto::user_register_result::REGISTER_FAILED_NAME_TAKEN, boost::optional<std::string>(), connection);
				}
			}
		);
	}

	void register_moudle::proto_write_user_register_response(int result_code, boost::optional<std::string> cert, connection_ptr connection)
	{
		proto::user_register_result result;
		result.set_result((proto::user_register_result::user_register_result_code)result_code);
		if (cert.is_initialized())
		{
			result.set_cert(cert.value());
		}
		connection->write_msg(encode(result));
	}

}
