#ifndef __XTEN_SINGLETON_H__
#define __XTEN_SINGLETON_H__
#include <memory>
namespace xten
{
    namespace
    {
        // c++11后静态局部变量的初始化是线程安全的
        template <class T, class Tag, int N>
        T &GetInstanceX()
        {
            static T t;
            return t;
        }
        template <class T, class Tag, int N>
        std::shared_ptr<T> &GetInstancePtr()
        {
            static std::shared_ptr<T> ret = std::make_shared<T>();
            return ret;
        }
    }
    // T 类型
    // Tag 为了创造多个实例对应的Tag
    // N 同一个Tag创造多个实例索引
    // 获取单例指针
    template <class T, class Tag = void, int N = 0>
    class Singleton
    {
    public:
        static T *GetInstance()
        {
            return &GetInstanceX<T, Tag, N>();
        }
    };
    // 获取单例智能指针
    template <class T, class Tag = void, int N = 0>
    class SingletonPtr
    {
    public:
        static std::shared_ptr<T> GetInstance()
        {
            return GetInstancePtr<T, Tag, N>();
        }
    };
}
#endif