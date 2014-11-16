
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <boost/noncopyable.hpp>
#include <boost/thread.hpp>

class http_server :  boost::noncopyable
{
public:
    http_server();
    ~http_server();
};

#endif // HTTP_SERVER_H
