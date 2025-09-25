#include "SemRWMutex.h"
#include <iostream>
#include <unistd.h>
#include <thread>
#include <string>
#include"ShmQueue.h"
static int tmp = 0;
#define KEY 120
int main()
{
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
    xten::ShmQueue::ptr x1=xten::ShmQueue::GetShmQueuePtr("/tmp",10,25,xten::EnumVisitModel::MulitPushMulitPop);
    std::cout<<x1->GetQueueSize()<<std::endl;
    // xten::ShmQueue::ShmQueControlBlock block;
    // queSize==32
    int ret1=x1->PushMessage("data",4);
    int ret2=x1->PushMessage("test",4);
    int ret3=x1->PushMessage("duck",4);
    std::cout<<ret1<<ret2<<ret3<<std::endl;
    // x1->PopMessage();
    // x1->PeekHeadMessage();
    // x1->DelHeadMessage();
    std::cout<<*x1<<std::endl;
    // auto x2=xten::ShmQueue<void,0>::GetInstance();
    // auto x3=xten::ShmQueue<void,1>::GetInstance();
    // auto x4=xten::ShmQueue<void,2>::GetInstance();
    // auto x5=xten::ShmQueue<void,1>::GetInstance();
    // auto x6=xten::ShmQueue<void,2>::GetInstance();
    return 0;
}