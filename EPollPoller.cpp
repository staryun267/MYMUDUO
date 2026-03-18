#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

//channel中的index_ 成员所表示的就是当前channel在Poller中的状态
const int kNew=-1;  //channel未添加到Poller中
const int kAdded=1; //channel已添加到Poller中
const int kDeleted=2;   //channel从Poller中删除

EPollPoller::EPollPoller(EventLoop *loop)
 : Poller(loop),
   epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
   events_(kInitEventListSize)
{
    if(epollfd_<0)
    {
        LOG_FATAL("epoll_create error:%d\n",errno);
    }
}
EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

//Poll就是监听fd,将发生事件的channel填写到EventLoop给定的容器中。
Timestamp EPollPoller::Poll(int timeoutMs,ChannelList *activeChannels)
{
    //实际上应该用LOG_DEBUG输出更为合理
    LOG_INFO("func=%s fd total count:%lu\n",__FUNCTION__,channels_.size());

    int numEvents=::epoll_wait(epollfd_,&*events_.begin(),events_.size(),timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if(numEvents>0)
    {
        LOG_INFO("%d events happend\n",numEvents);
        fillActiveChannels(numEvents,activeChannels);
        if(numEvents == events_.size())
        {
            events_.resize(events_.size()*2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout!\n",__FUNCTION__);
    }
    else
    {
        if(saveErrno != EINTR)
        {
            errno = saveErrno;
            LOG_ERROR("EPollPoller::Poller err!");
        }
    }
    return now;
}

//channel -> EventLoop的updateChannel -> Poller的updateChannel ->EPollPoller的updateChannel
/*          
            EventLoop
    ChannelList     Poller
                    ChannelMap<fd,channel>

*/
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s,fd=%d,events=%d,index=%d\n",__FUNCTION__,channel->fd(),channel->events(),index);

    if(index == kNew || index == kDeleted)
    {
        if(index == kNew)
        {
            int fd = channel->fd();
            channels_[fd]=channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD,channel);
    }
    else    //channel已经在Poller上注册过了
    {
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD,channel);
        }
    }
}
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);

    LOG_INFO("func=%s,fd=%d\n",__FUNCTION__,fd);

    int index = channel->index();
    if(index == kAdded)
    {
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);
}

//填写活跃连接
void EPollPoller::fillActiveChannels(int numEvents,ChannelList *activeChannels) const
{
    for(int i=0;i < numEvents; ++i)
    {
        Channel *channel=static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);     //EventLoop拿到了他的Poller返回的所有发生事件的channel列表
    }
}

//更新channel通道 epoll_ctl
void EPollPoller::update(int operation,Channel * channel)
{
    epoll_event event;
    memset(&event,0, sizeof event);
    int fd=channel->fd();

    event.events=channel->events();
    event.data.fd=fd;
    event.data.ptr=channel;

    if(::epoll_ctl(epollfd_,operation,fd,&event)<0)
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n",errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n",errno);
        }
    }
}
