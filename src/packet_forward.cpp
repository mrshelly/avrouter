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

	void packet_forward::process_packet(google::protobuf::Message* msg, connection_ptr connection, connection_manager&)
	{
		boost::any private_ptr = connection->retrive_module_private("routing_table");
		if( private_ptr.empty() )
		{
			private_ptr = boost::make_shared<routine_table_type>();
			connection->store_module_private("routine_table", private_ptr);
		}
		boost::shared_ptr<routine_table_type> routing_table = boost::any_cast<boost::shared_ptr<routine_table_type>>(private_ptr);

		proto::avPacket * pkt = dynamic_cast<proto::avPacket*>(msg);
		if( pkt->dest().domain() != m_thisdomain)
		{
			// TODO 暂时不实现非本域的转发
		}

		// 根据用户名找到连接
		auto forward_target = routing_table->find(pkt->dest().username());

		if( forward_target != routing_table->end() )
		{
			// 找到, 转发过去
			// TTL 减1
			if( pkt->time_to_live() > 1)
			{
				pkt->set_time_to_live( pkt->time_to_live() - 1 );
				forward_target->second->write_msg(encode(*pkt));
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
