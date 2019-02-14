/*
 * file:        qthread.h
 * description: assignment - simple emulation of POSIX threads
 * class:       CS 5600, Fall 2018
 */
#ifndef __QTHREAD_H__
#define __QTHREAD_H__

#include <sys/socket.h>

/* you need to define the following (typedef) types:
 *  qthread_t, qthread_mutex_t, qthread_cond_t
 * I'll assume you're defining them as aliases for structures of the
 *  same name (minus the "_t") for mutex and cond, and a pointer to
 *  'struct qthread' for qthread_t.
 */

/* forward reference. Define the contents of the structure in
 * qthread.c 
 */
struct qthread;
typedef struct qthread* qthread_t;

/* Due to broken-ness in C and the POSIX threads spec, when the
 * compiler includes "qthread.h" it needs to know the size
 * (i.e. contents) of the qthread_mutex_t and qthread_cond_t
 * structures. We hack around that by having a single field, a pointer
 * to the "implementation" structure that's defined only in qthread.c,
 * and allocated/deallocated in the _init() and _destroy functions.
 */
struct qthread_mutex {
    struct qthread_mutex_impl *impl;
};
struct qthread_cond {
    struct qthread_cond_impl *impl;
};
    
typedef struct qthread_mutex qthread_mutex_t;
typedef struct qthread_cond qthread_cond_t;

/* prototypes - see qthread.c for function descriptions
 */

void qthread_run(void);
qthread_t qthread_start(void (*f)(void*, void*), void *arg1, void *arg2);
qthread_t qthread_create(void* (*f)(void*), void *arg1);
void qthread_yield(void);
void qthread_exit(void *val);
void *qthread_join(qthread_t thread);
void qthread_mutex_init(qthread_mutex_t *mutex);
void qthread_mutex_lock(qthread_mutex_t *mutex);
void qthread_mutex_unlock(qthread_mutex_t *mutex);

void qthread_mutex_uninit(qthread_mutex_t* mutex);
void qthread_cond_init(qthread_cond_t *cond);
void qthread_cond_wait(qthread_cond_t *cond, qthread_mutex_t *mutex);
void qthread_cond_signal(qthread_cond_t *cond);
void qthread_cond_broadcast(qthread_cond_t *cond);

void qthread_cond_uninit(qthread_cond_t* cond);
void qthread_usleep(long int usecs);
ssize_t qthread_read(int sockfd, void *buf, size_t len);
int qthread_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t qthread_write(int sockfd, void *buf, size_t len);

unsigned get_usecs();

#endif
