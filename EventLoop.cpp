#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <errno.h>
#include <unistd.h>
#include <memory>

//防止一个线程创建多个EventLoop
__thread EventLoop* t_loopInThisThread = nullptr;

//定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs =10000;

//创建wakeupfd，用来notify唤醒subreactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0)
    {
        LOG_FATAL("eventfd err:%d\n",errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
     quit_(false),
     callingPendingFunctors_(false),
     threadId_(CurrentThread::tid()),
     poller_(Poller::newDefaultPoller(this)),
     wakeupFd_(createEventfd()),
     wakeupChannel_(new Channel(this,wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in Thread %d \n",this,threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this Thread %d",t_loopInThisThread,threadId_);
    }else
    {
        t_loopInThisThread=this;
    }

    //设置wakeupfd的事件类型以及发生事件后的回调操作，
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    //每一个EventLoop都将监听wakeup的EPOLLIN读事件了
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread=nullptr;
}

void EventLoop::loop()
{
    looping_=true;
    quit_=false;

    LOG_INFO("EventLoop %p start looping",this);
    while(!quit_)
    {
        activeChannels_.clear();
        //监听两类fd,一种是client的fd，一种是wakeupfd
        pollReturnTime_ = poller_->Poll(kPollTimeMs,&activeChannels_);
        for(Channel *channel : activeChannels_)
        {
            //Poller监听哪些channel发生了事件，然后上报给EventLoop，EventLoop通知channel处理事件
            channel->handleEvent(pollReturnTime_);
        }
        //当mainReactor想将channel提交到subReactor的EventLoop时会先将任务存储在pendingFunctors_中
        //当mainReactor调用eventfd唤醒EventLoop后，EventLoop会调用下面的函数，执行任务
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping \n",this);
    looping_ = false;
}

//退出事件循环  1.loop在自己的线程中调用quit  2.在非loop的线程中调用quit
void EventLoop::quit()
{
    quit_ = true;
    if(!isInLoopThread())   /*当在非loop的线程中调用quit时会唤醒loop的线程，若当前该线程正在处理事件则当其从Poll返回时，再次判断quit，便会退出循环；若它并没有处理事件而是阻塞在Poll上时，wakeup会唤醒它(handleread)，迫使他再次判断quit_*/
    {
        wakeup();
    }
}
//在当前Loop中执行cb
void EventLoop::runInLoop(Functor cb)
{
    if(isInLoopThread())    //在当前loop线程中
    {
        cb();
    }
    else    //不在，唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }
}
//把cb放入队列中，唤醒loop所在线程执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    //唤醒相应的，需要执行上面回调操作的loop线程
    //|| callingPendingFunctors_意思是，当前loop正在执行回调，但loop又有了新的回调。
    if(!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}

//用来唤醒loop所在线程，向wakeupfd写一个数据，wakeupchannel发生读事件，当前loop线程就会被唤醒。
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_,&one,sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8 \n",n);
    }
}

//EventLoop -》Poller
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}

//执行回调
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor : functors)
    {
        functor();  //执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_,&one,sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::handleRead() reads %lu bytes instead of 8",n);
    }
}