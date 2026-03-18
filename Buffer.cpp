#include "Buffer.h"

#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
/*
    从fd上读取数据，poller工作在LT模式
    Buffer缓冲区是有大小的，但从fd上读取数据的大小却不知道,tcp数据最终的大小
*/
ssize_t Buffer::readFd(int fd,int *saveErrno)
{
    char extrabuf[65536] = {0}; //栈上的内存空间
    struct iovec vec[2];
    const size_t writable = writeableBytes();
    //写入数据时，如果writable够用则直接写入，如果不够则写在extrabuf中
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;    //如果可写缓冲区的大小小于extrabuf的大小则选择，2(extrabuf + 缓冲区),否则选择可写缓冲区
    const ssize_t n = ::readv(fd,vec,iovcnt);
    if(n < 0)
    {
        *saveErrno = errno;
    }else if(n <= writable)
    {
        writerIndex_ += n;
    }
    else    //extrabuf中有数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf,n-writable);
    }
    return n;
}

//通过fd发送数据
ssize_t Buffer::writeFd(int fd,int *saveErrno)
{
    ssize_t n = ::write(fd,peek(),readableBytes());
    if( n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}