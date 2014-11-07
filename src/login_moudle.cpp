#include "login_moudle.hpp"
#include "user.pb.h"

namespace av_router {

	login_moudle::login_moudle()
	{}

	login_moudle::~login_moudle()
	{}

	void login_moudle::process_message(google::protobuf::Message*, connection_ptr, connection_manager&)
	{
		// TODO: 处理登陆.
	}

	void login_moudle::process_hello_message(google::protobuf::Message* hellomsg, connection_ptr, connection_manager&)
	{
		proto::client_hello * client_hello = dynamic_cast<proto::client_hello>(hellomsg);
		// TODO: 生成随机数然后返回
	}

}
