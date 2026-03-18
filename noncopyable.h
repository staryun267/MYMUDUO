#pragma once

/*
继承noncopyable的派生类不可以进行拷贝构造和赋值操作，但可以正常的进行构造和析构。
*/

class noncopyable
{
public:
    //删除拷贝构造
    noncopyable(const noncopyable&) = delete;
    //删除赋值操作
    noncopyable& operator=(const noncopyable&) = delete;
protected:
    //显式生成默认的构造函数和析构函数
    noncopyable() = default;
    ~noncopyable() = default;
};