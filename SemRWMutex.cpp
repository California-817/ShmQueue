#include "SemRWMutex.h"
#include <sys/ipc.h>
#include <string>
#include <errno.h>
#include <assert.h>
#include <exception>
#include <stdexcept>
#include <string.h>
#include <stdio.h>
namespace xten
{
    union semun
    {
        int val;               /* Value for SETVAL */
        struct semid_ds *buf;  /* Buffer for IPC_STAT, IPC_SET */
        unsigned short *array; /* Array for GETALL, SETALL */
        struct seminfo *__buf; /* Buffer for IPC_INFO
                                  (Linux-specific) */
    };
    SemRWMutex::SemRWMutex(const std::string &pathname, int proj_id)
        : _key(-1), _semId(-1)
    {
        // 获取key
        _key = ftok(pathname.c_str(), proj_id);
        if (_key == -1)
        {
            printf("ftok called failed, errno=%d,errstr=%s\n", errno, strerror(errno));
            throw std::runtime_error("ftok error: " + std::string(strerror(errno)));
        }
        init(_key);
    }
    SemRWMutex::SemRWMutex(key_t key)
        : _key(key)
    {
        if (_key == -1)
        {
            printf("ftok called failed, errno=%d,errstr=%s\n", errno, strerror(errno));
            throw std::runtime_error("ftok error: " + std::string(strerror(errno)));
        }
        init(_key);
    }
    void SemRWMutex::init(key_t key)
    {
        semun arg;
        // 创建读写信号量
        _semId = semget(key, 2, IPC_CREAT | IPC_EXCL | 0666);
        if (_semId == -1)
        {
            printf("semget called failed, errno=%d,errstr=%s\n", errno, strerror(errno));
            if (errno == EEXIST)
            {
                // 因为已经存在了一个信号量
                printf("sem which key=%d has been exists\n", key);
                // 链接信号量---括号优先级问题
                if ((_semId = semget(key, 2, IPC_CREAT | 0666)) == -1) // 链接失败
                    throw std::runtime_error("semget error: " + std::string(strerror(errno)));
                // 链接成功
            }
            else
                throw std::runtime_error("semget error: " + std::string(strerror(errno)));
        }
        else
        {
            unsigned short array[2] = {0, 0};
            arg.array = array;
            // 此处创建信号量集---对信号量进行初始化
            if (semctl(_semId, 0, SETALL, arg) == -1)
            {
                throw std::runtime_error("semclt init val error: " + std::string(strerror(errno)));
            }
        }
    }
    SemRWMutex::~SemRWMutex()
    {
        //Immediately remove the semaphore set,   立即删除这个信号量集 唤醒所有正在信号量集上等待的进程，错误码为 EIDRM
        //awakening all processes blocked in semop(2) calls on the set (with an error return and errno  set to  EIDRM).
        // semctl(_semId,2,IPC_RMID);------>不同于共享内存的删除，这里立即删除，因此不能RMID，只能在外部手动删除
        printf("SemRWMutex::~SemRWMutex\n");
    }
    // 阻塞的加锁接口
    // 读加锁
    void SemRWMutex::RLock()
    {
        // struct sembuf contains:
        // unsigned short sem_num;  /* semaphore number */   操作的信号量编号
        //    short          sem_op;   /* semaphore operation */  + 增加信号量   - 减少信号量  0 等待信号量变为0
        //    short          sem_flg;  /* operation flags */  SEM_UNDO---利用内核防止死锁
        // 读信号量+1 等待写信号量为0
        struct sembuf sops[2] = {{1, 0, SEM_UNDO}, {0, 1, SEM_UNDO}};
        int ret = -1;
        do
        {
            ret = semop(_semId, sops, 2);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1 && errno != EINTR)
            printf("RLock failed: errstr=%s\n", strerror(errno));
    }
    // 写加锁
    void SemRWMutex::WLock()
    {
        // 等待读信号量为0 并且写信号量为0  写信号量+1
        struct sembuf sops[3] = {{1, 0, SEM_UNDO}, {0, 0, SEM_UNDO}, {1, 1, SEM_UNDO}};
        int ret = -1;
        do
        {
            ret = semop(_semId, sops, 3);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1 && errno != EINTR)
            printf("WLock failed: errstr=%s\n", strerror(errno));
    }
    // 读解锁
    void SemRWMutex::RUnLock()
    {
        // 读信号量--
        struct sembuf sops[1] = {{0, -1, SEM_UNDO}};
        int ret = -1;
        do
        {
            ret = semop(_semId, sops, 1);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1 && errno != EINTR)
            printf("RUnLock failed: errstr=%s\n", strerror(errno));
    }
    // 写解锁
    void SemRWMutex::WUnLock()
    {
        // 写信号量--
        struct sembuf sops[1] = {{1, -1, SEM_UNDO}};
        int ret = -1;
        do
        {
            ret = semop(_semId, sops, 1);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1 && errno != EINTR)
            printf("WUnLock failed: errstr=%s\n", strerror(errno));
    }
    // 非阻塞加锁接口
    bool SemRWMutex::TryRLock()
    {
        struct sembuf sops[2] = {{1, 0, SEM_UNDO | IPC_NOWAIT}, {0, 1, SEM_UNDO | IPC_NOWAIT}};
        int ret = -1;
        ret = semop(_semId, sops, 2);
        if (ret == -1)
        {
            // 尝试加锁失败
            if (ret == EAGAIN)
            {
                return false;
            }
            printf("TryRLock failed: errstr=%s\n", strerror(errno));
            return false;
        }
        return true;
    }
    bool SemRWMutex::TryWLock()
    {
        struct sembuf sops[3] = {{1, 0, SEM_UNDO | IPC_NOWAIT}, {0, 0, SEM_UNDO | IPC_NOWAIT}, {1, 1, SEM_UNDO | IPC_NOWAIT}};
        int ret = -1;
        ret = semop(_semId, sops, 3);
        if (ret == -1)
        {
            // 尝试加锁失败
            if (ret == EAGAIN)
            {
                return false;
            }
            printf("TryWLock failed: errstr=%s\n", strerror(errno));
            return false;
        }
        return true;
    }
    // 获取key值
    int SemRWMutex::GetKey() const
    {
        return _key;
    }
    // 获取semid
    int SemRWMutex::GetSemId() const
    {
        return _semId;
    }

} // namespace xten
