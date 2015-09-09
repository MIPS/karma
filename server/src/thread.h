/*
 * thread.h - TODO enter description
 *
 * (c) 2011 Janis Danisevskis <janis@sec.t-labs.tu-berlin.de>
 * (c) 2011 Steffen Liebergeld <steffen@sec.t-labs.tu-berlin.de>
 *
 * This file is part of the Karma VMM and distributed under the terms of the
 * GNU General Public License, version 2.
 *
 * Please see the file COPYING-GPL-2 for details.
 */
/*
 * thread.h
 *
 *  Created on: 24.09.2010
 *      Author: janis
 */

#ifndef THREAD_HPP_
#define THREAD_HPP_

#include <pthread.h>
#include <pthread-l4.h>
#include <string>
#include <cstdio>
#include <l4/sys/debugger.h>
#include <cstdlib>
#include <l4/sys/scheduler>
#include <l4/re/env>

//#include <boost/bind.hpp>

namespace karma{

class PTCondBool{
private:
    pthread_cond_t _cond;
    pthread_mutex_t _mutex;
    bool _condition;
public:
    explicit PTCondBool(const bool state): _condition(state){
        pthread_cond_init(&_cond, NULL);
        pthread_mutex_init(&_mutex, NULL);
    }
    ~PTCondBool(){
        pthread_mutex_destroy(&_mutex);
        pthread_cond_destroy(&_cond);
    }

    void set(const bool & new_state){
        pthread_mutex_lock(&_mutex);
        if(_condition != new_state){
            _condition = new_state;
            signal();
        }
        pthread_mutex_unlock(&_mutex);
    }
    PTCondBool & operator = (const bool & new_state){
        set(new_state);
        return *this;
    }
protected:
    void signal(){
        pthread_cond_signal(&_cond);
    }
    void broadcast(){
        pthread_cond_broadcast(&_cond);
    }
public:
    void waitConditional(const bool condition){
        pthread_mutex_lock(&_mutex);
        while(condition != _condition){
            pthread_cond_wait(&_cond, &_mutex);
        }
        pthread_mutex_unlock(&_mutex);
    }

    void set_waitConditional(const bool condition){
        pthread_mutex_lock(&_mutex);
        _condition = !condition;
        while(condition != _condition){
            pthread_cond_wait(&_cond, &_mutex);
        }
        pthread_mutex_unlock(&_mutex);
    }
};

class Runable{
public:
    virtual void run(L4::Cap<L4::Thread> & thread) = 0;
    virtual ~Runable(){}
};

//template <typename FUNCTOR>
//class BindRunable: public Runable{
//private:
//    FUNCTOR _f;
//public:
//    BindRunable(FUNCTOR & f)
//    :   _f(f)
//    {
//    }
//    virtual void run(L4::Cap<L4::Thread> & thread){
//        _f(thread);
//    }
//};

class Thread : protected Runable
{
private:
    unsigned _prio;
    pthread_t _pthread;
    std::string _name;
    Runable * _runable;
public:
    /**
     * \param prio Priority
     * \param name an optional name that is used for the debugger
     */
    Thread(unsigned prio=2, const std::string name = "")
    : _prio(prio)
    , _name(name)
    , _runable(NULL)
    {
    }
    Thread(Runable * runable, unsigned prio=2, const std::string name = "")
    : _prio(prio)
    , _name(name)
    , _runable(runable)
    {
    }
    virtual ~Thread(){
        delete _runable;
    }
    void start(unsigned int cpu=0)
    {
        _start_thread(cpu);
    }

    L4::Cap<L4::Thread> get_thread()
    {
        L4::Cap<L4::Thread> thread(pthread_getl4cap(_pthread));
        return thread;
    }

protected:
    virtual void run(L4::Cap<L4::Thread> & thread){
        if(_runable) _runable->run(thread);
        else {
            printf("running karma::Thread::run without runnable\n");
            exit(1);
        }
    }
private:

    void setup()
    {
        L4::Cap<L4::Thread> thread(pthread_getl4cap(pthread_self()));
        run(thread);
    }

    static void * __thread_func(void * data){
        static_cast<Thread*>(data)->setup();
        return NULL;
    }

    void _start_thread(unsigned int cpu);

};

} // namespace karma
#endif /* THREAD_HPP_ */
