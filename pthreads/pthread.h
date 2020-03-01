#pragma once

#include <cassert>

#include <thread>
#include <mutex>
#include <condition_variable>

typedef void* (*pthread_start_routine_t)(void*);

struct pthread_t
{
    std::thread* thread{nullptr};
};

struct pthread_mutex_t
{
    std::mutex* mutex{nullptr};
    std::unique_lock<std::mutex>* lock{nullptr};
};

struct pthread_cond_t
{
    std::condition_variable* cond;
};

inline int pthread_create(pthread_t* thread, void*, pthread_start_routine_t start, void *arg)
{
    assert( thread );
    assert( !thread->thread );
    thread->thread = new std::thread(start, arg);
    return 0;
}

inline int pthread_join(pthread_t& thread, void** retval)
{
    assert( thread.thread );

    thread.thread->join();
    delete thread.thread;
    thread.thread = nullptr;

    if( retval )
    {
        *retval = nullptr;
    }
        
    return 0;
}

inline int pthread_mutex_init(pthread_mutex_t* mutex, void*)
{
    assert( mutex );
    assert( !mutex->mutex );
    assert( !mutex->lock );

    mutex->mutex = new std::mutex;
    mutex->lock = nullptr;

    return 0;
}

inline int pthread_mutex_destroy(pthread_mutex_t* mutex)
{
    assert( mutex );
    assert( mutex->mutex );
    assert( !mutex->lock );

    delete mutex->mutex;
    mutex->mutex = nullptr;

    return 0;
}

inline int pthread_mutex_lock(pthread_mutex_t* mutex)
{
    assert( mutex );
    assert( mutex->mutex );
    assert( !mutex->lock );

    mutex->lock = new std::unique_lock<std::mutex>(*mutex->mutex);

    return 0;
}

inline int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
    assert( mutex );
    assert( mutex->mutex );
    assert( mutex->lock );
    
    delete mutex->lock;
    mutex->lock = nullptr;

    return 0;
}

inline int pthread_cond_init(pthread_cond_t* cond, void*)
{
    assert( cond );
    assert( !cond->cond );

    cond->cond = new std::condition_variable;

    return 0;
}

inline int pthread_cond_destroy(pthread_cond_t* cond)
{
    assert( cond );
    assert( cond->cond );

    delete cond->cond;
    cond->cond = nullptr;

    return 0;
}

inline int pthread_cond_broadcast(pthread_cond_t *cond)
{
    assert( cond );
    assert( cond->cond );

    cond->cond->notify_all();

    return 0;
}

inline int pthread_cond_signal(pthread_cond_t *cond)
{
    assert( cond );
    assert( cond->cond );

    cond->cond->notify_one();

    return 0;
}

inline int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex)
{
    assert( cond );
    assert( cond->cond );

    assert( mutex );
    assert( mutex->mutex );
    assert( mutex->lock );

    cond->cond->wait(*mutex->lock);

    return 0;
}
