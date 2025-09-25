#include "ShmQueue.h"
#include <iostream>
#include <sys/ipc.h>
#include <string.h>
#include <assert.h>
#include <sys/shm.h>
#include <sstream>
namespace xten
{
    // 大小对齐到2的n次幂
    static size_t roundUpToPowerOfTwo(size_t v)
    {
        if (v == 0)
            return 1;
        v--;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        if (sizeof(size_t) > 4)
        { // 如果size_t是64位
            v |= v >> 32;
        }
        v++;
        return v;
    }
    // 错误码转string
    static const char *errorCode2String(ShmQueErrorCode code)
    {
        switch (code)
        {
#define XX(CODE)                           \
    case ShmQueErrorCode::CODE:            \
        return "ShmQueueErrorCode=" #CODE; \
        break;
            XX(QueueFailedKey)
            XX(QueueFailedSharedMemory)
            XX(QueueParameterInvaild)
#undef XX
        default:
            break;
        }
        return "ShmQueueErrorCode=UnKnown";
    }
    // 构造函数
    ShmQueue::ShmQueue(key_t key, size_t quesize, int shmId, void *shmPtr,
                       EnumCreateModel newOrLink, EnumVisitModel visitModule)
        : _shmPtr(shmPtr), _newOrLink(newOrLink)
    {
        _controlBlock = new (shmPtr) ShmQueControlBlock();
        _quePtr = (BYTE *)shmPtr + sizeof(ShmQueControlBlock);
        _controlBlock->key = key;
        _controlBlock->queSize = quesize;
        _controlBlock->shmId = shmId;
        _controlBlock->vtModule = visitModule;
        initLock();
    }
    // 如果是link链接到一个已经启动的消息队列,应该调用这个构造函数---防止 [控制块] 的值被重置
    ShmQueue::ShmQueue(ShmQueControlBlock *cblock, EnumCreateModel newOrLink)
        : _shmPtr((void *)(cblock)), _newOrLink(newOrLink)
    {
        _controlBlock = cblock;
        _quePtr = (BYTE *)cblock + sizeof(ShmQueControlBlock);
        initLock();
    }
    ShmQueue::~ShmQueue()
    {
        if (_controlBlock)
        {
            key_t key = _controlBlock->key;
            _controlBlock->~ShmQueControlBlock();
            // 销毁占用的那块共享内存
            destroySharedMemory(_shmPtr, key);
        }
        // 锁的销毁
        if (_headMtx)
        {
            delete _headMtx;
            _headMtx = nullptr;
        }
        if (_tailMtx)
        {
            delete _tailMtx;
            _tailMtx = nullptr;
        }
    }
    // 放入消息
    int ShmQueue::PushMessage(const void *msg, DATA_SIZE_TYPE msglength)
    {
        if (!msg || msglength <= 0)
        {
            std::cout << errorCode2String(ShmQueErrorCode::QueueParameterInvaild) << std::endl;
            return (int)(ShmQueErrorCode::QueueParameterInvaild);
        }
        // 0.根据访问模式判断是否加锁
        WLockGuard lock(_tailMtx); // 空不加锁
        // 1.获取空闲空间大小
        size_t freeSize = getFreeSize();
        if (freeSize < msglength + sizeof(DATA_SIZE_TYPE))
        {
            // log
            return (int)(ShmQueErrorCode::QueueNoFreeSize);
        }
        // 2.确保了空间足够，开始放数据
        // 2.1放入固定长度的length字段
        BYTE *tmpDst = _quePtr;
        int tmptail = _controlBlock->tailIdx;
        BYTE *tmpLen = (BYTE *)(&msglength);
        for (int i = 0; i < sizeof(DATA_SIZE_TYPE); i++)
        {
            tmpDst[tmptail] = tmpLen[i];
            tmptail = (tmptail + 1) & (_controlBlock->queSize-1); // 存长度空间可能在头尾
        }
        // 2.2放msg----有两种情况  连续 or 头尾
        DATA_SIZE_TYPE part1Size = std::min(msglength, _controlBlock->queSize - _controlBlock->tailIdx);
        memcpy((void *)(tmpDst + tmptail), msg, (size_t)part1Size);
        DATA_SIZE_TYPE part2Size=msglength-part1Size;
        if(part2Size>0)
        {
            //数据在头尾----直接在队列起始位置放下剩余数据
            memcpy((void*)(tmpDst),(const void*)((BYTE*)msg+part1Size), (size_t)(part2Size));
        }
        //3.数据拷贝完---更新索引位置 [在更新索引位置之前，需要保证数据全部写入完毕，使用写内存屏障保障]
        sfence();
        //更新tail索引
        _controlBlock->tailIdx=(tmptail+msglength) & (_controlBlock->queSize-1);
        return (int)(ShmQueErrorCode::QueueOk);
    }
    // 取出消息
    int ShmQueue::PopMessage(void *buffer, size_t bufLength)
    {
    }
    // 获取消息拷贝---不改变索引位置
    int ShmQueue::PeekHeadMessage(void *buffer, size_t bufLength)
    {
    }
    // 删除头部消息---改变索引位置
    int ShmQueue::DelHeadMessage()
    {
    }
    // 根据访问模式决定锁的init
    void ShmQueue::initLock()
    {
        assert(_controlBlock);
        if (_controlBlock->vtModule == EnumVisitModel::MulitPushMulitPop ||
            _controlBlock->vtModule == EnumVisitModel::MulitPushSinglePop)
        {
            // 多线程push
            _tailMtx = new SemRWMutex(_controlBlock->key + 1);
        }
        if (_controlBlock->vtModule == EnumVisitModel::MulitPushMulitPop ||
            _controlBlock->vtModule == EnumVisitModel::SinglePushMulitPop)
        {
            // 多线程pop
            _headMtx = new SemRWMutex(_controlBlock->key + 2);
        }
    }
    // 获取空闲空间大小
    size_t ShmQueue::getFreeSize()
    {
        if (_controlBlock->headIdx <= _controlBlock->tailIdx)
        {
            return _controlBlock->queSize - (_controlBlock->tailIdx - _controlBlock->headIdx) - REMAIN_SIZE;
        }
        else
        {
            return _controlBlock->headIdx - _controlBlock->tailIdx - REMAIN_SIZE;
        }
    }
    // 删除共享内存--detach
    bool ShmQueue::destroySharedMemory(void *shmPtr, key_t key)
    {
        if (!shmPtr || key < 0)
        {
            std::cout << "destroySharedMemory failed" << std::endl;
            return false;
        }
        // 1.获取到这块内存
        int shmid = shmget(key, 0, 0666);
        if (shmid == -1)
        {
            // 获取失败
            std::cout << "destroySharedMemory at shmget failed" << std::endl;
            return false;
        }
        //  struct shmid_ds {
        //    struct ipc_perm shm_perm;    /* Ownership and permissions */
        //    size_t          shm_segsz;   /* Size of segment (bytes) */
        //    time_t          shm_atime;   /* Last attach time */
        //    time_t          shm_dtime;   /* Last detach time */
        //    time_t          shm_ctime;   /* Creation time/time of last
        //    modification via shmctl() */
        //    pid_t           shm_cpid;    /* PID of creator */
        //    pid_t           shm_lpid;    /* PID of last shmat(2)/shmdt(2) */
        //    shmatt_t        shm_nattch;  /* No. of current attaches */   attach该共享内存的进程数量
        //    ...
        //    };
        struct shmid_ds info;
        // 先获取一下信息
        int ret = shmctl(shmid, IPC_STAT, &info); // 不考虑是否失败
        // 删除共享内存块
        // detach
        if (shmdt(shmPtr) == -1)
        {
            // detach失败
            std::cout << "destroySharedMemory at shmdt failed,errstr=" << strerror(errno) << std::endl;
            return false;
        }
        // The segment will actually be destroyed only after the last process  detaches  it  只是标记为删除
        if (-1 == shmctl(shmid, IPC_RMID, NULL)) // IPC_RMID不填充info
        {
            // 删除失败
            std::cout << "destroySharedMemory at shmctl(IPC_RMID) failed,errstr=" << strerror(errno) << std::endl;
            return false;
        }
        if (!ret)
        {
            std::cout << "Success destroy SharedMempry, shmid_ds.shm_segsz=" << info.shm_segsz << " bytes, "
                      << "shmid_ds.shm_cpid=" << info.shm_cpid << ", shmid_ds.shm_nattch=" << info.shm_nattch - 1 << "(remain)" << std::endl;
        }
        return true;
    }
    // 获取共享内存
    void *ShmQueue::getSharedMemory(key_t key, int &shmid, EnumCreateModel &newOrLink, size_t size)
    {
        shmid = shmget(key, size, 0666 | IPC_CREAT | IPC_EXCL); // 成功的时候一定是创建
        if (shmid == -1)
        {
            // 其他类型失败
            if (errno != EEXIST)
            {
                std::cout << "getSharedMemory failed,errorStr=" << strerror(errno) << std::endl;
                return nullptr;
            }
            // 已经存在共享内存
            std::cout << "SharedMemory has been exists" << std::endl;
            if ((shmid = shmget(key, size, 0666 | IPC_CREAT)) == -1)
            {
                // 链接失败 先获取shmid 进行删除这个共享内存后重新创建
                shmid = shmget(key, 0, 0666);
                if (shmid == -1)
                {
                    // 获取都失败
                    std::cout << "SharedMemory has been exists , link failed and get shmid failed" << std::endl;
                    return nullptr;
                }
                // 获取成功，删除重新创建
                std::cout << "First remove already exists SharedMemory" << std::endl;
                if (shmctl(shmid, IPC_RMID, NULL))
                {
                    // 删除失败
                    std::cout << "Remove already exists SharedMemory failed,errorStr=" << strerror(errno) << std::endl;
                    return nullptr;
                }
                // 删除成功
                std::cout << "Remove already exists SharedMemory success" << std::endl;
                if ((shmid = shmget(key, size, 0666 | IPC_CREAT)) == -1)
                {
                    // 创建仍然失败
                    std::cout << "Remove already exists SharedMemory success, but Create failed, errstr="
                              << strerror(errno) << std::endl;
                    return nullptr;
                }
                newOrLink = EnumCreateModel::NewShmQue;
            }
            else
            {
                // 链接已经存在的成功
                std::cout << "Link exists SharedMemory success" << std::endl;
                newOrLink = EnumCreateModel::LinkShmQue;
            }
        }
        else
        {
            std::cout << "Create  a new SharedMemory success" << std::endl;
            newOrLink = EnumCreateModel::NewShmQue;
        }
        // 进行共享内存的attach
        void *shmptr = shmat(shmid, nullptr, 0);
        if (shmptr == (void *)-1)
        {
            // attach失败
            std::cout << "attach SharedMemory failed,errstr=" << strerror(errno) << std::endl;
            return nullptr;
        }
        // attach成功
        return shmptr;
    }
    // 获取一个进程安全共享内存消息队列实例(非单例)
    ShmQueue *ShmQueue::GetShmQueue(const std::string &pathname, int proj_id,
                                    size_t size, EnumVisitModel visitModule)
    {
        // 1.生成key
        key_t key = ftok(pathname.c_str(), proj_id);
        if (key == -1)
        {
            // 生成key失败
            std::cout << errorCode2String(ShmQueErrorCode::QueueFailedKey) << std::endl;
            return nullptr;
        }
        // 2.获取共享内存
        EnumCreateModel createM;
        int shmid = -1;
        //// 2.1将quesize对齐到2的n次方
        size = roundUpToPowerOfTwo(size);
        void *shmPtr = ShmQueue::getSharedMemory(key, shmid, createM, size + sizeof(ShmQueControlBlock));
        if (shmPtr == nullptr)
        {
            // 获取失败
            std::cout << errorCode2String(ShmQueErrorCode::QueueFailedSharedMemory) << std::endl;
            return nullptr;
        }
        // 3.创建该消息队列---分情况调用不同构造函数
        ShmQueue *shmque = nullptr;
        switch (createM)
        {
        case EnumCreateModel::NewShmQue:
            shmque = new ShmQueue(key, size, shmid, shmPtr, createM, visitModule);
            break;
        case EnumCreateModel::LinkShmQue:
            shmque = new ShmQueue((ShmQueue::ShmQueControlBlock *)shmPtr, createM);
        default:
            break;
        }
        return shmque;
    }
    ShmQueue::ptr ShmQueue::GetShmQueuePtr(const std::string &pathname, int proj_id,
                                           size_t size, EnumVisitModel visitModule)
    {
        return std::shared_ptr<ShmQueue>(ShmQueue::GetShmQueue(pathname, proj_id, size, visitModule));
    }
    std::string ShmQueue::PrintShmQueInfo() const
    {
        std::stringstream ss;
        // ss<<"test";
        return ss.str();
    }
    std::ostream &operator<<(std::ostream &os, const ShmQueue &queue)
    {
        os << queue.PrintShmQueInfo();
        return os;
    }
} // namespace xten
