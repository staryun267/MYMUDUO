#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;
//事件循环，主要包含两大类，Channel，Poller(epoll的抽象)
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    //开启事件循环
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    //在当前Loop中执行cb
    void runInLoop(Functor cb);
    //把cb放入队列中，唤醒loop所在线程执行cb
    void queueInLoop(Functor cb);

    //用来唤醒loop所在线程
    void wakeup();

    //EventLoop -》Poller
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    //判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
private:
    void handleRead();  //wake up
    void doPendingFunctors();   //执行回调
    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_; //原子操作，通过CAS实现
    std::atomic_bool quit_; //标识退出loop循环
    const pid_t threadId_;   //记录当前loop所在线程ID
    Timestamp pollReturnTime_;  //poller返回发生事件的channel的时间点

    std::unique_ptr<Poller> poller_;

    int wakeupFd_;  //主要作用，当main reactor获取到新的用户的channel，通过轮询算法选择一个subreactor，通过该成员唤醒subreactor
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;

    std::atomic_bool callingPendingFunctors_;   //标识当前loop是否有需要执行的回调操作
    std::vector<Functor> pendingFunctors_;  //存储loop需要执行的所有回调操作
    std::mutex mutex_;  //互斥锁，用来保护上面vector的线程安全操作
};