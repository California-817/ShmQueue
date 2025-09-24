#ifndef __XTEN_NOCOPYABLE_H__
#define __XTEN_NOCOPYABLE_H__
// 禁止拷贝基类
namespace xten
{
    class nocopyable
    {
    public:
        nocopyable() = default;
        nocopyable(const nocopyable &) = delete;
        nocopyable(nocopyable &&) = delete;
        nocopyable &operator=(const nocopyable &) = delete;
        nocopyable &operator=(nocopyable &&) = delete;
    };
} // namespace xten

#endif