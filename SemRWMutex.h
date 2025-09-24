#ifndef __XTEN_SEM_RWMTX_H__
#define __XTEN_SEM_RWMTX_H__
#include <sys/sem.h>
#include <memory>
#include "nocopyable.hpp"
// 基于System V信号量实现的进程读写锁
namespace xten
{
    // 读写锁
    class SemRWMutex : public nocopyable
    {
    public:
        typedef std::shared_ptr<SemRWMutex> ptr;
        SemRWMutex(const std::string &pathname, int proj_id);
        SemRWMutex(key_t key);
        ~SemRWMutex();
        // 阻塞的加锁接口
        // 读加锁
        void RLock();
        // 写加锁
        void WLock();
        // 读解锁
        void RUnLock();
        // 写解锁
        void WUnLock();
        // 非阻塞加锁接口
        bool TryRLock();
        bool TryWLock();
        // 获取key值
        int GetKey() const;
        // 获取semid
        int GetSemId() const;

    private:
        void init(key_t key);

    private:
        int _key;   // 生成id的唯一key
        int _semId; // 读锁的信号量id
    };
    // 自动加解锁的LockGuard
    class RLockGuard
    {
    public:
        RLockGuard()
            : _mtx(nullptr), _isLocked(false)
        {
        }
        RLockGuard(SemRWMutex *mtx)
            : _mtx(mtx), _isLocked(false)
        {
            if (_mtx)
            {
                _mtx->RLock(); // 阻塞式加读锁
                _isLocked = true;
            }
        }
        void Lock()
        {
            if (!_isLocked && _mtx)
            {
                _mtx->RLock(); // 加读锁
                _isLocked = true;
            }
            // 已经加锁了
        }
        void UnLock()
        {
            if (_isLocked && _mtx)
            {
                _mtx->RUnLock();
                _isLocked = false;
            }
        }
        ~RLockGuard()
        {
            if (_isLocked && _mtx)
            {
                _mtx->RUnLock();
                _isLocked = false;
                _mtx = nullptr;
            }
        }

    private:
        bool _isLocked;   // 是否上锁
        SemRWMutex *_mtx; // 锁指针
    };
    //加解写锁的guard
    class WLockGuard
    {
    public:
        WLockGuard()
            : _mtx(nullptr), _isLocked(false)
        {
        }
        WLockGuard(SemRWMutex *mtx)
            : _mtx(mtx), _isLocked(false)
        {
            if (_mtx)
            {
                _mtx->WLock(); // 阻塞式加读锁
                _isLocked = true;
            }
        }
        void Lock()
        {
            if (!_isLocked && _mtx)
            {
                _mtx->WLock(); // 加读锁
                _isLocked = true;
            }
            // 已经加锁了
        }
        void UnLock()
        {
            if (_isLocked && _mtx)
            {
                _mtx->WUnLock();
                _isLocked = false;
            }
        }
        ~WLockGuard()
        {
            if (_isLocked && _mtx)
            {
                _mtx->WUnLock();
                _isLocked = false;
                _mtx = nullptr;
            }
        }
    private:
        bool _isLocked;   // 是否上锁
        SemRWMutex *_mtx; // 锁指针
    };
} // namespace xten
#endif