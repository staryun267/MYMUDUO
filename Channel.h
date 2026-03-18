#pragma once
/*
EventLoop,channel,poller对应的是,reactor模型中的Demultiplex(事件分发器)
Channel理解为通道，封装sockfd和想监听的事件，EPOLLIN EPOLLOUT
还绑定了poller绑定的具体事件
*/

#include <functional>
#include <memory>
#include "Timestamp.h"
#include "noncopyable.h"

class EventLoop;

class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop,int fd);
    ~Channel();
    
    //fd得到poller通知以后，处理事件
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb) { readCallback_=std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_=std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_=std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_=std::move(cb); }

    //防止channel被手动remove掉，channel还在执行回调函数
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    int events() const { return events_; }

    //将poller返回的事件设置到channel
    void set_revents(int revt) { revents_=revt; }

    //设置fd相应的事件状态
    void enableReading() { events_|=kReadEvent; update();}
    void disableReading() { events_&=~kReadEvent; update();}
    void enableWriting() { events_|=kWriteEvent; update();}
    void disableWriting() { events_&=~kWriteEvent; update();}
    void disableAll() { events_=kNoneEvent; update();}

    //返回fd当前相应的事件状态
    bool isNoneEvent() const { return events_==kNoneEvent; }
    bool isReading() const { return events_ & kReadEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }

    int index() const { return index_;}
    void set_index(int idx) { index_=idx; }

    //one loop per thread
    EventLoop* ownerloop() const { return loop_; }
    void remove();

private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;   //事件循环
    const int fd_;  //poller监听的fd
    int events_;    //注册fd感兴趣的事件
    int revents_;   //poller返回的fd发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    //因为Channel通道里面能获知fd发生的具体事件revents,所以CHannel负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};