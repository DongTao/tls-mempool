#ifndef _THREAD_LOCAL_MEMORY_POOL_H_
#define _THREAD_LOCAL_MEMORY_POOL_H_

#include <vector>
#include <boost/noncopyable.hpp>
#include <boost/pool/pool.hpp>
#include <boost/thread/tss.hpp>

namespace tlsmempool {
// boost tls实现的线程绑定内存池
// 适合多线程场景下先频繁申请小对象，后批量释放的场景，不支持有参构造函数
template<class T, class P>
class ThreadLocalMemoryPool : private boost::noncopyable {
private:
    typedef T chunk_type; // 分配对象类型
    typedef P pool_type;  // 内存池类型，目前仅支持boost::pool
private:
    boost::thread_specific_ptr<pool_type> thread_local_pool;
private:
    static const int kSuccess   =  0; // 成功
    static const int kFailed    = -1; // 失败
    static const int kNoMemPool = -2; // 内存池丢失
    static const int kFromElse  = -3; // 非当前内存池分配
private:
    // lazy load chunk_type memory pool
    pool_type* GetMemoryPool() {
        pool_type* mem_pool =  NULL;
        try {
            mem_pool = thread_local_pool.get();
            if (mem_pool == NULL) {
                mem_pool = new pool_type(sizeof(chunk_type));
                thread_local_pool.reset(mem_pool);
            }
        } catch (...) {
            delete mem_pool;
            thread_local_pool.reset();
            mem_pool = NULL;
        }
        return mem_pool;
    }
public:
    ThreadLocalMemoryPool() {}
    ~ThreadLocalMemoryPool() {}

public:
    // 从内存池申请一个对象
    chunk_type* Create() {
        chunk_type* obj = NULL;
        pool_type* mem_pool = NULL;
        try {
            mem_pool = GetMemoryPool();
            if (mem_pool == NULL) {
                return NULL;
            }
            obj = reinterpret_cast<chunk_type*>(mem_pool->malloc());
            if (!obj) {
                return NULL;
            }
            new (obj) chunk_type();
        } catch (...) {
            if (obj != NULL) {
                mem_pool->free(obj);
            };
            obj = NULL;
        }
        return obj;
    }
    // 从内存池申请N个连续对象数组
    chunk_type* Create(int count) {
        chunk_type* obj_array = NULL;
        pool_type* mem_pool = NULL;
        try {
            mem_pool = GetMemoryPool();
            if (mem_pool == NULL) {
                return NULL;
            }
            obj_array  = reinterpret_cast<chunk_type*>(mem_pool->ordered_malloc(count));
            if (!obj_array ) {
                return NULL;
            }
            for (int idx = 0; idx < count; ++idx) {
                new (obj_array + idx) chunk_type();
            }
        } catch (...) {
            if (obj_array != NULL) {
                mem_pool->ordered_free(obj_array, count);
            };
            obj_array = NULL;
        }
        return obj_array;
    }
    // 销毁一个对象，与Create对应
    int Destroy(chunk_type* ptr) {
        if (ptr == NULL) return ThreadLocalMemoryPool::kSuccess;
        pool_type* mem_pool = GetMemoryPool();
        if (mem_pool == NULL) {
            return ThreadLocalMemoryPool::kNoMemPool;
        }
        if (mem_pool->is_from(ptr) == false) {
            return ThreadLocalMemoryPool::kFromElse;
        }
        mem_pool->free(ptr);
        return ThreadLocalMemoryPool::kSuccess;
    }
    // 销毁N个对象，与Create数组对应
    int Destroy(chunk_type* ptr, int count) {
        if (ptr == NULL) return ThreadLocalMemoryPool::kSuccess;
        pool_type* mem_pool = GetMemoryPool();
        if (mem_pool == NULL) {
            return ThreadLocalMemoryPool::kNoMemPool;
        }
        if (mem_pool->is_from(ptr) == false) {
            return ThreadLocalMemoryPool::kFromElse;
        }
        mem_pool->ordered_free(ptr, count);
        return ThreadLocalMemoryPool::kSuccess;
    }
    // 快速清空内存池
    int PurgeMemory() {
        pool_type* mem_pool = GetMemoryPool();
        if (mem_pool == NULL) {
            return ThreadLocalMemoryPool::kNoMemPool;
        }
        mem_pool->purge_memory();
        return ThreadLocalMemoryPool::kSuccess;
    }
    // 销毁内存池
    void ReleaseMemoryPool() {
        thread_local_pool.reset();
    }
};

// 自定义模板Allocator，使用ThreadLocalMemoryPool进行线程安全的指针内存分配与回收
// T: 指针类型，P: 内存池类型
template<class T, class P = boost::pool<> >
class ThreadLocalPointerAllocator : public std::allocator<T> {
public:
    typedef std::allocator<T> base_type;
    typedef typename base_type::size_type        size_type;
    typedef typename base_type::pointer          pointer;
    typedef ThreadLocalMemoryPool<T, P>          tls_mempool_type;

    template<class Other>
    struct rebind {
        typedef ThreadLocalPointerAllocator<Other> other;
    };
    // 构造函数必须实现
    ThreadLocalPointerAllocator() {}
    ThreadLocalPointerAllocator(ThreadLocalPointerAllocator<T> const&) {}
    ThreadLocalPointerAllocator<T>& operator=(ThreadLocalPointerAllocator<T> const&) { return (*this); }
    // idiom: Coercion by Member Template
    template<class Other>
    ThreadLocalPointerAllocator(ThreadLocalPointerAllocator<Other> const&) {}
    // idiom: Coercion by Member Template
    template<class Other>
    ThreadLocalPointerAllocator<T>& operator=(ThreadLocalPointerAllocator<Other> const&) { return (*this); }
    pointer allocate(size_type count) {
        T* ret = reinterpret_cast<T*>(tls_mempool_type::Create(count));
        if (!ret) {
            std::__throw_bad_alloc();
        }
        return ret;
    }
    void deallocate(pointer ptr, size_type count) {
        tls_mempool_type::Destroy(ptr, count);
    }
};
} // tlsmempool
#endif
