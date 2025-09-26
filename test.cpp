#include "SemRWMutex.h"
#include <iostream>
#include <unistd.h>
#include <thread>
#include <string>
#include <atomic>
#include <assert.h>
#include "ShmQueue.h"
static int tmp = 0;
#define KEY 120
static std::atomic_ulong count = 0;
void test()
{
    // 5线程
    xten::ShmQueue::ptr shmque = xten::ShmQueue::GetShmQueuePtr("/tmp", 100, 64, xten::EnumVisitModel::MulitPushMulitPop);
    assert(shmque);
    // 生产数据--2thread
    std::thread t1 = std::thread([]()
                                 {
    xten::ShmQueue::ptr shmque = xten::ShmQueue::GetShmQueuePtr("/tmp", 100, 64, xten::EnumVisitModel::MulitPushMulitPop);
        sleep(1);
        std::string s("shmQueueTest1,SeqNum=");
        for(int i=0;i<1000000;)
        {
            // sleep(1);
            std::string msg=s;
            msg+=std::to_string(i);
            if(shmque->PushMessage(msg.c_str(),msg.length())==0)
                i++;
        } });
    std::thread t2 = std::thread([]()
                                 {
    xten::ShmQueue::ptr shmque = xten::ShmQueue::GetShmQueuePtr("/tmp", 100, 64, xten::EnumVisitModel::MulitPushMulitPop);
        sleep(1);
          std::string s("shmQueueTest2,SeqNum=");
  for(int i=0;i<1000000;)
  {
            // sleep(1);
      std::string msg=s;
      msg+=std::to_string(i);
      if(0==shmque->PushMessage(msg.c_str(),msg.length()))
        i++;
  } });
  //消费数据--3thread
    std::thread t3 = std::thread([]()
                                 {
    xten::ShmQueue::ptr shmque = xten::ShmQueue::GetShmQueuePtr("/tmp", 100, 64, xten::EnumVisitModel::MulitPushMulitPop);
        sleep(1);

    while(count<2000000)
        {
        char buffer[1024]={0};
        int ret=-1;
        if((ret=shmque->PopMessage(buffer,sizeof(buffer)))>=0)
        {
            if(ret>0)
            {
            std::cout<<buffer<<std::endl;
            count++;}
        }
        else
        {
            break;
        }
    } });
    std::thread t4 = std::thread([]()
                                 {
    xten::ShmQueue::ptr shmque = xten::ShmQueue::GetShmQueuePtr("/tmp", 100, 64, xten::EnumVisitModel::MulitPushMulitPop);
        sleep(1);

    while (count<2000000)
        {
            char buffer[1024] = {0};
            int ret=-1;
            if ((ret=shmque->PopMessage(buffer, sizeof(buffer))) >= 0)
            {
                if(ret>0)
                {
                std::cout << buffer << std::endl;
                count++;}
            }
            else
            {
                break;
            }
        } });
    //等待5s，让前4个线程先进行生产消费，再启动这个消费线程，模拟启动队列后再添加生产或消费者的情况
    sleep(5);
    std::thread t5 = std::thread([]()
                                 {
    xten::ShmQueue::ptr shmque = xten::ShmQueue::GetShmQueuePtr("/tmp", 100, 64, xten::EnumVisitModel::MulitPushMulitPop);
        sleep(1);

    while(count<2000000)
     {
     char buffer[1024]={0};
     int ret=-1;
     if((ret=shmque->PopMessage(buffer,sizeof(buffer)))>=0)
     {
         if(ret>0)
         {
         std::cout<<buffer<<std::endl;
         count++;}
     }
     else
     {
         break;
     }
 } });
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    //确保消息没有丢失
    std::cout << "count=" << count << std::endl;
}
int main()
{
    test();
    // std::thread t1 = std::thread([]()
    //  {
    // xten::SemRWMutex mtx("/tmp",KEY);
    // for(int i=0;i<100000;i++)
    // {
    //
    // mtx.WLock();
    // tmp++;
    // mtx.WUnLock();
    // } });
    // std::thread t2 = std::thread([]()
    //  {
    // xten::SemRWMutex mtx("/tmp",KEY);
    // for(int i=0;i<100000;i++)
    //   {
    //   mtx.WLock();
    //   tmp++;
    //   mtx.WUnLock();
    //   } });
    // t1.join();
    // t2.join();
    // xten::SemRWMutex mtx("/tmp",KEY);
    // std::cout << "tmp=" << tmp << std::endl;
    // for (int i = 0; i < 2; i++)
    // {
    // int val = semctl(mtx.GetSemId(), i, GETVAL);
    // if (val == -1)
    // {
    // perror("semctl GETVAL");
    // exit(EXIT_FAILURE);
    // }
    // printf("Semaphore %d value: %d\n", i, val);
    // }
    // xten::ShmQueue::ptr x1 = xten::ShmQueue::GetShmQueuePtr("/tmp", 10, 25, xten::EnumVisitModel::MulitPushMulitPop);
    // std::cout<<x1->GetQueueSize()<<std::endl;
    // xten::ShmQueue::ShmQueControlBlock block;
    // queSize==32
    // int ret1 = x1->PushMessage("dataa", 5);
    // std::cout << *x1 << std::endl;
    // int ret2 = x1->PushMessage("tes", 3);
    // std::cout << *x1 << std::endl;
    // char buffer[1024] = {0};
    // x1->PopMessage(buffer, sizeof(buffer));
    // int ret3 = x1->PushMessage("duck", 4);
    // std::cout << *x1 << std::endl;
    // int ret4 = x1->PeekHeadMessage(buffer, sizeof(buffer));
    // std::cout << ret1 << ret2 << ret3 << ret4 << std::endl;
    // std::cout << buffer << std::endl;
    // x1->PopMessage();
    // x1->PeekHeadMessage();
    // x1->DelHeadMessage();
    //  std::cout << *x1 << std::endl;
    //    x1->DelHeadMessage();
    // std::cout << *x1 << std::endl;
    // x1->DelHeadMessage();
    // std::cout << *x1 << std::endl;
    // auto x2=xten::ShmQueue<void,0>::GetInstance();
    // auto x3=xten::ShmQueue<void,1>::GetInstance();
    // auto x4=xten::ShmQueue<void,2>::GetInstance();
    // auto x5=xten::ShmQueue<void,1>::GetInstance();
    // auto x6=xten::ShmQueue<void,2>::GetInstance();
    return 0;
}