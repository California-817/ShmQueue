#ifndef __XTEN_SHM_QUEUE_H__
#define __XTEN_SHM_QUEUE_H__
#include <memory>
#include <string>
#include "SemRWMutex.h"
#include "nocopyable.hpp"
// 线程安全的共享内存消息队列

// cpu缓存行大小
#define CPU_CACHELINE_SIZE 64

// 定义编译器屏障---仅禁止编译器的指令重排
#define compiler_barrier() __asm__ __volatile__("" ::: "memory")

// 定义cpu内存屏障
// 1.mfence 全屏障 （读写操作都静止重排序，都可见）
#define mfence() __asm__ __volatile__("mfence" ::: "memory")
// 2.sfence 写屏障  (写操作在屏障前完成，并且写操作对其他核心可见)
#define sfence() __asm__ __volatile__("sfence" ::: "memory")
// 3.lfence 读屏障  (读操作在屏障后完成，并且读操作读到的是最新的值)
#define lfence() __asm__ __volatile__("lfence" ::: "memory")
namespace xten
{
// 进行内存对齐
#define ALIGNED_CACHELINE_SIZE __attribute__((aligned(CPU_CACHELINE_SIZE)))

    // 定义单字节大小类型
    typedef unsigned char BYTE;
    // 定义存储数据长度的数据类型
    typedef size_t DATA_SIZE_TYPE;
// 定义固定大小的额外空间
#define REMAIN_SIZE 8
    // 读写访问模式---元素大小1字节
    enum class EnumVisitModel : unsigned char
    {
        SinglePushSinglePop = 0, // 单push单pop
        SinglePushMulitPop = 1,  // 单push多pop
        MulitPushSinglePop = 2,  // 多push单pop
        MulitPushMulitPop = 3,   // 多push多pop
    };
    // 首次创建 or 链接已经存在
    enum class EnumCreateModel : unsigned char
    {
        NewShmQue = 0,  // 创建一个全新ShmQue
        LinkShmQue = 1, // 链接到已经存在的ShmQue
    };
    // 这个消息队列的一些错误码
    enum class ShmQueErrorCode
    {
        QueueOk = 0,                        // 无错误
        QueueFailedKey = -1,                // 生成key失败
        QueueFailedSharedMemory = -2,       // 获取共享内存失败
        QueueParameterInvaild = -3,         // 参数错误
        QueueNoFreeSize = -4,               // 无剩余空间
        QueueDataError = -5,                // 数据长度字段不足
        QueueDataLengthError = -6,          // 数据长度字段有错
        QueueBufferLengthInsufficient = -7, // 获取消息时缓冲区长度不足
    };
    class ALIGNED_CACHELINE_SIZE ShmQueue : public nocopyable
    {
    private:
        // 这个共享内存消息队列对应的头部控制块---记录一些信息
        // 1) 这里读写索引用int类型,cpu可以保证对float,double和long除外的基本类型的读写是原子的,保证一个线程不会读到另外一个线程写到一半的值
        struct ShmQueControlBlock
        {
            volatile int headIdx = 0;               // 队列头部索引
            char memoryInsert1[CPU_CACHELINE_SIZE]; // 填充缓存行 防止false sharing
            volatile int tailIdx = 0;               // 队列尾部索引
            char memoryInsert2[CPU_CACHELINE_SIZE];
            size_t queSize = 0; // 队列空间大小/Byte
            char memoryInsert3[CPU_CACHELINE_SIZE];
            key_t key = -1; // key值
            char memoryInsert4[CPU_CACHELINE_SIZE];
            int shmId = -1; // key对应的shmid
            char memoryInsert5[CPU_CACHELINE_SIZE];
            EnumVisitModel vtModule; // 访问模式
        } ALIGNED_CACHELINE_SIZE;

    public:
        typedef std::shared_ptr<ShmQueue> ptr;

        // 获取一个进程安全共享内存消息队列实例(非单例)---智能指针
        // queSize会被对齐到2的n次幂
        static std::shared_ptr<ShmQueue> GetShmQueuePtr(const std::string &pathname, int proj_id,
                                                        size_t quesize, EnumVisitModel visitModule = EnumVisitModel::MulitPushMulitPop);
        // 获取一个进程安全共享内存消息队列实例(非单例)---裸指针
        // queSize会被对齐到2的n次幂
        static ShmQueue *GetShmQueue(const std::string &pathname, int proj_id,
                                     size_t quesize, EnumVisitModel visitModule = EnumVisitModel::MulitPushMulitPop);

        // 析构
        ~ShmQueue();

        // 一些获取属性接口
        // QueSize
        size_t GetQueueSize() const { return _controlBlock->queSize; }
        // shmid
        int GetShmId() const { return _controlBlock->shmId; }
        // visitModule
        EnumVisitModel GetVisitModel() const { return _controlBlock->vtModule; }
        // 创建或者链接
        EnumCreateModel GetCreateModel() const { return _newOrLink; }

        // 放入消息 on succecss ret=0 ; on failed ret<0
        int PushMessage(const void *msg, DATA_SIZE_TYPE msglength);
        // 取出消息 on succecss ret=sizeof(message) ; on failed ret<0
        int PopMessage(void *buffer, size_t bufLength);
        // 获取头部消息拷贝---不改变索引位置 on succecss ret=sizeof(message) ; on failed ret<0
        int PeekHeadMessage(void *buffer, size_t bufLength);
        // 删除头部消息---改变索引位置 on succecss ret=sizeof(message) ; on failed ret<0
        int DelHeadMessage();
        // 打印共享内存消息队列的属性信息
        std::string PrintShmQueInfo() const;

    private:
        ShmQueue(key_t key, size_t quesize, int shmId, void *shmPtr,
                 EnumCreateModel newOrLink, EnumVisitModel visitModule = EnumVisitModel::MulitPushMulitPop);
        // 如果是link链接到一个已经启动的消息队列,应该调用这个构造函数---防止 [控制块] 的值被重置
        ShmQueue(ShmQueControlBlock *cblock, EnumCreateModel newOrLink);
        // 获取共享内存的接口--系统分配内存大小为4KB的整数倍
        static void *getSharedMemory(key_t key, int &shmid, EnumCreateModel &newOrLink, size_t size);
        // 删除共享内存----detach and rmid
        static bool destroySharedMemory(void *shmPtr, key_t key);
        // 根据访问模式决定锁的init
        void initLock();
        // 获取空闲空间的大小
        size_t getFreeSize() const;
        // 获取数据大小
        size_t getDataSize() const;

    private:
        ShmQueControlBlock *_controlBlock; // 头部控制块地址
        void *_shmPtr;                     // 共享内存起始地址
        BYTE *_quePtr;                     // 消息队列的起始地址 (两个地址相隔一个控制块距离)

        SemRWMutex *_headMtx = nullptr; // 头部锁
        SemRWMutex *_tailMtx = nullptr; // 尾部锁

        EnumCreateModel _newOrLink; // 创建或者链接
    };
    std::ostream &operator<<(std::ostream &os, const ShmQueue &queue);
} // namespace xten
#endif