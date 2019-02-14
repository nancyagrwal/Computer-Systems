/*
 * file:        qthread.c
 * description: assignment - simple emulation of POSIX threads
 * class:       CS 5600, Fall 2018
 */

/* a bunch of includes which will be useful */

#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include "qthread.h"
#include <errno.h>
#include <sys/select.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#define STACK_SIZE (16 * 1024)

struct linkedlist;
typedef struct linkedlist linkedlist_t;

struct qthread {

    void* stack;                //Stack
    void* stack_ptr_location;
    void* return_val;           //Return value
    int fd;                     //file descriptor
    int done;                  //Flag to indicate if thread is done (1 for done)
    linkedlist_t* waiting_threads;     //The next thread waiting for this thread
    int read_write;              //Flag for R=1/W=0
};

qthread_t current;       // indicates current stack pointer.

/* prototypes for stack.c and switch.s
 * see source files for additional details
 */
extern void switch_to(void** location_for_old_sp, void* new_value);

extern void* setup_stack(int* stack, void* func, void* arg1, void* arg2);

typedef struct listitem {
    qthread_t thr;
    struct listitem* next;
} listitem_t;


typedef struct linkedlist {
    listitem_t* head;
    listitem_t* tail;
} linkedlist_t;


struct linkedlist* make_list() {
    linkedlist_t* ll = malloc(sizeof(linkedlist_t));

    ll->head = NULL;
    ll->tail = NULL;

    return ll;
}

linkedlist_t* active_queue;  // queue of active threads.
linkedlist_t* sleeping_queue;    // queue of sleeping threads.
linkedlist_t* io_wait_queue;  // queue of threads waiting for I/O.

/* These are your actual mutex and cond structures - you'll allocate
 * them in qthread_mutex_init / cond_init and free them in the
 * corresponding _destroy functions.
 */
struct qthread_mutex_impl {
    int locked;
    linkedlist_t* waiting_threads;
};
struct qthread_cond_impl {
    linkedlist_t* waiting_threads;
};


/**
 * Function to append the given thread to the thread queue.
 *
 * @param lst is the thread queue.
 * @param thread is the thread pointer.
 */
void list_queue(linkedlist_t* list, qthread_t thread) {
    listitem_t* item = malloc(sizeof(listitem_t));
    item->thr = thread;
    item->next = NULL;

    if (list->head == NULL) {
        list->head = item;
        list->tail = item;
    } else {
        list->tail->next = item;
        list->tail = item;
    }
}

/**
 * Function to pop the given thread from the thread queue.
 *
 * @param list is the thread queue.
 */
qthread_t list_dequeue(linkedlist_t* list) {
    if (list->head == NULL) {
        return NULL;
    } else {
        listitem_t* current = list->head;
        list->head = current->next;
        if (list->head == NULL) {
            list->tail = NULL;
        }
        qthread_t thr = current->thr;
        free(current);
        return thr;
    }
}

/**
 * Function to check whether the thread queue is empty.
 *
 * @param list is the thread queue.
 */
int is_list_empty(struct linkedlist* list) {
    return (list->head == NULL);
}

void init_active_queue() {
    if (active_queue == NULL) {
        active_queue = make_list();
    }
    if (sleeping_queue == NULL) {
        sleeping_queue = make_list();
    }
    if (io_wait_queue == NULL) {
        io_wait_queue = make_list();
    }
}

void* main_stack;
int destroy_stack;
int is_in_thread_currently = 0;

/**
 * Exit a thread (temporarily) and go to the main thread.
 * Should only be called from non-main threads
 * @param free_stack 1 if we want to deallocate the stack after exiting,
 *                   0 if we want to be able to go into this thread again.
 */
void switch_to_main(int free_stack) {
    is_in_thread_currently = 0;
    destroy_stack = free_stack;
    switch_to(&current->stack_ptr_location, main_stack);
}

void switch_from_main(qthread_t thread_to_switch_to) {
    is_in_thread_currently = 1;
    current = thread_to_switch_to;
    switch_to(&main_stack, current->stack_ptr_location);
}

/** function to select an available file descriptor:
 *select() allows a program to monitor multiple file descriptors,
 *waiting until one or more of the file descriptors become "ready"
 *for some class of I/O operation
 */
void wait_for_io() {

    int no_fds = 0;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;

    fd_set read_fdset;
    fd_set write_fdset;

    FD_ZERO(&read_fdset);
    FD_ZERO(&write_fdset);

    listitem_t* c = io_wait_queue->head;
    while (c != NULL) {
        no_fds++;
        if (c->thr->read_write == 1)
            FD_SET(c->thr->fd, &read_fdset);
        else
            FD_SET(c->thr->fd, &write_fdset);
        c = c->next;
    }
    select(no_fds, &read_fdset, &write_fdset, NULL, &tv);
};

/**
 * This should always be run from the main thread
 */
void thread_scheduler() {
    // qthread_t *this = current;
//again:
    while (!is_list_empty(active_queue) || !is_list_empty(io_wait_queue) || !is_list_empty(sleeping_queue)) {
        qthread_t nextThread = list_dequeue(active_queue);

        if (nextThread == NULL) {
            wait_for_io();
            while (!is_list_empty(io_wait_queue)) {
                list_queue(active_queue, list_dequeue(io_wait_queue));
            }
            // usleep(10000); //TODO is this necessary?
            while (!is_list_empty(sleeping_queue)) {
                struct qthread* thr = list_dequeue(sleeping_queue);
                list_queue(active_queue, thr);
            }
        }

        if (nextThread != NULL) {
            switch_from_main(nextThread);
            if (destroy_stack) {
                free(nextThread->stack);
                while (!is_list_empty(nextThread->waiting_threads)) {
                    list_queue(active_queue, list_dequeue(nextThread->waiting_threads));
                }
            }
        }
    }
};


/* qthread_start, qthread_create - see video for difference.
 * (function passed to qthread_create is allowed to return)
 */
qthread_t qthread_start(void (* f)(void*, void*), void* arg1, void* arg2) {
    init_active_queue();

    void* stack = malloc(STACK_SIZE);

    void* stack_top = stack + STACK_SIZE;
    stack_top = setup_stack(stack_top, f, arg1, arg2);

    qthread_t qth = malloc(sizeof(struct qthread));

    qth->stack = stack;
    qth->stack_ptr_location = stack_top;
    qth->done = 0;
    qth->fd = (int) NULL;
    qth->read_write = -1;
    qth->waiting_threads = make_list();

    list_queue(active_queue, qth);

    return qth;
}

void qthread_exiter(void* f, void* arg1) {
    void* (* func)(void*) = f;
    qthread_exit(func(arg1));
}

qthread_t qthread_create(void* (* f)(void*), void* arg1) {
    qthread_start(qthread_exiter, f, arg1);
}


/* qthread_run - run until the last thread exits
 */
void qthread_run(void) {
    thread_scheduler();
}

/* qthread_yield - yield to the next runnable thread.
 */
void qthread_yield(void) {
    list_queue(active_queue, current);

    switch_to_main(0);
}

/* qthread_exit, qthread_join - exit argument is returned by
 * qthread_join. Note that join blocks if the thread hasn't exited
 * yet, and is allowed to crash if the thread doesn't exist.
 */
void qthread_exit(void* val) {

    qthread_t this = current;
    this->return_val = val;
    this->done = 1;

    switch_to_main(1);

};

/**
 * Exit argument is returned by qthread_join.
 * Note that join blocks if thread hasn't exited yet,
 * and may crash if thread doesn't exist.
 *
 * @param thread is the thread for current thread to join.
 * @return thread exit value.
 */
void* qthread_join(qthread_t thread) {
    if (is_in_thread_currently) {
        list_queue(thread->waiting_threads, current);
    }

    while (!thread->done) {
        if (is_in_thread_currently) {
            switch_to_main(0);
        } else {
            thread_scheduler();
        }
    }

    return thread->return_val;
}

/* Mutex functions
 */

/**
 * Initiate the mutex.
 *
 * @param mutex is the mutex pointer
 */
void qthread_mutex_init(qthread_mutex_t* mutex) {
    mutex->impl = malloc(sizeof(struct qthread_mutex));
    mutex->impl->locked = 0;
    mutex->impl->waiting_threads = make_list();
}

/**
 * Function to lock the mutex.
 *
 * @param mutex is the mutex pointer
 */
void qthread_mutex_lock(qthread_mutex_t* mutex) {
    if (mutex->impl->locked == 0) {
        mutex->impl->locked = 1;
    } else {
        list_queue(mutex->impl->waiting_threads, current);
        switch_to_main(0);
    }
}

/**
 * Function to unlock the mutex.
 *
 * @param mutex is the mutex pointer
 */
void qthread_mutex_unlock(qthread_mutex_t* mutex) {
    if (is_list_empty(mutex->impl->waiting_threads)) {
        mutex->impl->locked = 0;
    } else {
        qthread_t t = list_dequeue(mutex->impl->waiting_threads);
        list_queue(active_queue, t);
    }
}

void qthread_mutex_uninit(qthread_mutex_t* mutex) {
    mutex->impl->locked = 0;
    free(mutex->impl->waiting_threads);
    free(mutex->impl);
}

/* Condition variable functions
 */

/**
 * Function to initiate the condition variables.
 *
 * @param cond is the condition variable pointer
 */
void qthread_cond_init(qthread_cond_t* cond) {
    cond->impl = malloc(sizeof(struct qthread_cond));
    cond->impl->waiting_threads = make_list();
}

/**
 * Wait the condition variable.
 *
 * @param cond is the condition variable pointer
 * @param mutex is the mutex pointer
 */
void qthread_cond_wait(qthread_cond_t* cond, qthread_mutex_t* mutex) {
    if (mutex == NULL || cond == NULL) {
        return;
    }
    list_queue(cond->impl->waiting_threads, current);
    qthread_mutex_unlock(mutex);
    switch_to_main(0);
    qthread_mutex_lock(mutex);
}

/**
 * Function to signal the condition variable.
 *
 * @param cond is the condition variable pointer.
 */
void qthread_cond_signal(qthread_cond_t* cond) {
    if (cond == NULL) return;
    if (!is_list_empty(cond->impl->waiting_threads)) {
        list_queue(active_queue, list_dequeue(cond->impl->waiting_threads));
    }
}

/**
 * Function to broadcast all the condition variable.
 *
 * @param cond is the condition variable pointer
 */
void qthread_cond_broadcast(qthread_cond_t* cond) {
    if (cond == NULL) return;
    while (!is_list_empty(cond->impl->waiting_threads)) {
        list_queue(active_queue, list_dequeue(cond->impl->waiting_threads));
    }
}

void qthread_cond_uninit(qthread_cond_t* mutex) {
    free(mutex->impl->waiting_threads);
    free(mutex->impl);
}


/* Helper function for the POSIX replacement API - you'll need to tell
 * time in order to implement qthread_usleep.
 */
unsigned get_usecs(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}


/* POSIX replacement API. These are all the functions (well, the ones
 * used by the sample application) that might block.
 *
 * If there are no runnable threads, your scheduler needs to block
 * waiting for one of these blocking functions to return. You should
 * probably do this using the select() system call, indicating all the
 * file descriptors that threads are blocked on, and with a timeout
 * for the earliest thread waiting in qthread_usleep()
 */

/* qthread_usleep - yield to next runnable thread, making arrangements
 * to be put back on the active list after 'usecs' timeout.
 */
void qthread_usleep(long int usecs) {
    unsigned i = usecs + get_usecs();
    while (get_usecs() < i) {
        list_queue(sleeping_queue, current);
        switch_to_main(0);
    }
}

/* make sure that the file descriptor is in non-blocking mode, try to
 * read from it, if you get -1 / EAGAIN then add it to the list of
 * file descriptors to go in the big scheduling 'select()' and switch
 * to another thread.
 */
ssize_t qthread_read(int fd, void* buf, size_t len) {
    /* set non-blocking mode every time. If we added wrappers to
     * open(), socket(), etc. we could set non-blocking mode from the
     * beginning, but this is a lot simpler (if less efficient). Do
     * this for _write and _accept, too.
     */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int readResult;

    current->read_write = 1;
    current->fd = fd;

    // Keep trying to read the file if we get a block,
    // Yield to other threads
    // Error out if there is some other error
    while ((readResult = read(fd, buf, len)) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        list_queue(io_wait_queue, current);
        switch_to_main(0);
    }

    return readResult;
}

/**
 * write function for the given thread.
 *
 * @param fd is the file descriptor
 * @param buf is the writing buffer
 * @param len is the length to be written.
 */
ssize_t qthread_write(int fd, void* buf, size_t len) {
    /* your code here */
    int value, temp = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, temp | O_NONBLOCK);

    current->read_write = 0;
    current->fd = fd;

    while ((value = write(fd, buf, len)) == -1 && errno == EAGAIN) {
        list_queue(io_wait_queue, current);
        switch_to_main(0);
    }
    return value;
}

int qthread_accept(int fd, struct sockaddr* addr, socklen_t* addrlen) {
    /* your code here. Note that accept() counts as a 'read' for the
     * select call.
     */
    int value, temp = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, temp | O_NONBLOCK);
    current->fd = fd;
    current->read_write = 1;
    while ((value = accept(fd, addr, addrlen)) == -1 && errno == EAGAIN) {
        list_queue(io_wait_queue, current);
        switch_to_main(0);
    }
    return value;
}

