#include "thread_local_memory_pool.h"
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <string>

using namespace std;

static const int kObjCount = 10000000;
static const int kThreadCount = 2;

typedef tlsmempool::ThreadLocalMemoryPool<string, boost::pool<> > TLSMemPool;
static TLSMemPool pool;

void AllocateALot() {
    for (int i = 0; i < kObjCount; ++i) {
        if (pool.Create() == NULL) {
            cout << "create failed" <<endl;
            return;
        }
    }
    pool.PurgeMemory();
    pool.ReleaseMemoryPool();
}

void NewALot() {
    string* str_array[kObjCount];
    for (int i = 0; i < kObjCount; ++i) {
        str_array[i] = new string();
    }
    for (int i = 0; i < kObjCount; ++i) {
        delete str_array[i];
    }
}

int main(void) {
    boost::thread_group thread_group;
    boost::posix_time::ptime tbeg(boost::posix_time::microsec_clock::universal_time());
    for (int i = 0; i < kThreadCount; ++i) {
        thread_group.create_thread(&AllocateALot);
    }
    thread_group.join_all();
    boost::posix_time::ptime tend(boost::posix_time::microsec_clock::universal_time());
    cout << "AllocateALot cost " << (double)(tend-tbeg).total_microseconds()/1e3 << endl;

    boost::posix_time::ptime tbeg1(boost::posix_time::microsec_clock::universal_time());
    for (int i = 0; i < kThreadCount; ++i) {
        thread_group.create_thread(&AllocateALot);
    }
    thread_group.join_all();
    boost::posix_time::ptime tend1(boost::posix_time::microsec_clock::universal_time());
    cout << "NewALot cost " << (double)(tend1-tbeg1).total_microseconds()/1e3 << endl;

    return 0;
}
