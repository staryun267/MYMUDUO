#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,IPPROTO_TCP);
    if(sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d sockfd create fatal:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop,const InetAddress &listenAddr,bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()),   //socket
      acceptChannel_(loop,acceptSocket_.fd()),
      listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);  //bind
    //TcpServer::start() Acceptor.listen 有新用户的连接，执行一个回调(sockfd -> channel -> subloop)
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead,this));
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

void Acceptor::listen()
{
    printf("=== Acceptor::listen() 执行了!fd=%d\n", acceptChannel_.fd());
    listenning_ = true;
    acceptSocket_.listen(); //listen
    acceptChannel_.enableReading();
}

//listenfd有事件发生了，就是有新用户连接
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);   //accept
    if(connfd > 0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd,peerAddr);    //轮询找到subloop，唤醒，分发，当前的新客户端的Channel
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept error:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
        if(errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d accept error:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
        }
    }

}