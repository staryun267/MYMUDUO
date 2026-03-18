#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <sys/epoll.h>

const int Channel::kNoneEvent=0;
const int Channel::kReadEvent=EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent=EPOLLOUT;

Channel::Channel(EventLoop *loop,int fd)
    : loop_(loop),fd_(fd),events_(0),revents_(0),index_(-1),tied_(false)
    {}

Channel::~Channel(){}

//在什么时候调用?   一个TcpConnection连接创建的时候,TcpConnection -> channel
void Channel::tie(const std::shared_ptr<void>& obj)
{
    tie_=obj;
    tied_=true;
}

//当改变channel所表示的fd的events之后，update负责更改poller中fd相应的事件epoll_ctl
void Channel::update()
{
    //通过channel所属的EventLoop,调用poller对应的方法,改变fd的events
    loop_->updateChannel(this);
}

//在channel所属的EventLoop中把channel删除
void Channel::remove()
{
    loop_->removeChannel(this);
}

//fd得到poller通知以后，处理事件
void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_)
    {
        std::shared_ptr<void>guard=tie_.lock();
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }else
    {
        handleEventWithGuard(receiveTime);
    }
}

//根据poller通知的channel具体发生的事件，由channel负责调用回调函数
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("Channel handleEvent revents:%d\n",revents_);
    if((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
    }

    if(revents_ & EPOLLERR)
    {
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI))
    {
        if(readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if(revents_ & EPOLLOUT)
    {
        if(writeCallback_)
        {
            writeCallback_();
        }
    }
}