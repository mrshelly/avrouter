#include "login_moudle.hpp"

namespace av_router {

	login_moudle::login_moudle()
	{}

	login_moudle::~login_moudle()
	{}

	void login_moudle::process_message(google::protobuf::Message*, connection_ptr, connection_manager&)
	{
		// TODO: 处理登陆.
	}

	void login_moudle::process_hello_message(google::protobuf::Message*, connection_ptr, connection_manager&)
	{
		// TODO: 生成随机数然后返回
	}

}
