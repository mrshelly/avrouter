#include <boost/any.hpp>

#include "packet_forward.hpp"
#include "message.pb.h"

#include <openssl/dh.h>
#include <openssl/aes.h>

namespace av_router {

	packet_forward::packet_forward(boost::asio::io_service& io)
		: m_io_service(io)
	{
	}

	packet_forward::~packet_forward()
	{}

	void packet_forward::connection_notify(int type, connection_ptr connection, connection_manager&)
	{
		try
		{
			if( type != 0)
			{
				// 从 routing table 里删掉这个连接
				boost::any username_ = connection->retrive_module_private("username");
				std::string username = boost::any_cast<std::string>(username);

				m_routing_table.erase(username);
			}
		}
		catch(const boost::bad_any_cast &)
		{}
	}

	void packet_forward::process_packet(google::protobuf::Message* msg, connection_ptr connection, connection_manager&)
	{
		proto::avPacket * pkt = dynamic_cast<proto::avPacket*>(msg);
		if( pkt->dest().domain() != m_thisdomain)
		{
			// TODO 暂时不实现非本域的转发
		}

		// 根据用户名找到连接
		auto forward_target = m_routing_table.find(pkt->dest().username());
		connection_ptr conn = forward_target->second.lock();
		if(forward_target != m_routing_table.end() && conn)
		{
			// 找到, 转发过去
			// TTL 减1
			if( pkt->time_to_live() > 1)
			{
				pkt->set_time_to_live(pkt->time_to_live() - 1);
				conn->write_msg(encode(*pkt));
			}
			else
			{
				// TODO 发送 ttl = 0 消息
			}
		}
		else
		{
			// 没找到，回一个 aGMP 消息报告 no route to host
		}
		// TODO 根据目的地址转发消息
	}
}
