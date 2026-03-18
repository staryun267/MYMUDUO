#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <unordered_map>
#include <vector>

class EventLoop;
class Channel;

//muduo库中多路事件分发器的核心 IO复用模块
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;
    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    //给所有IO复用保留统一接口
    virtual Timestamp Poll(int timeoutMs,ChannelList *activeChannels)=0;
    virtual void updateChannel(Channel *channel)=0;
    virtual void removeChannel(Channel *channel)=0;

    //判断channel是否在当前Poller中
    bool hasChannel(Channel *channel) const;

    //EventLoop可以通过该接口获取IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop *loop);
protected:
    //map的key: sockfd  value:sockfd所属的channel
    using ChannelMap = std::unordered_map<int,Channel*>;
    ChannelMap channels_;
private:
    EventLoop *ownerLoop_;
};