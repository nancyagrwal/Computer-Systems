
#include <stdio.h>
#include "qthread.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>


// define a mutex m1:
qthread_mutex_t m1;

// define 3 condition variables:
qthread_cond_t con1, con2, con3;


void* yield_single_thread_multiple_times(void* arg) {
    // printf("yield single thread multiple times \n");
    int i;
    for (i = 0; i < 3; i++) {
        qthread_yield();
    }
    qthread_exit(arg);
}

void yield_multiple_threads_with_join(void) {
    // printf("testing yield and join with multiple threads\n");
    qthread_t thr = qthread_create(yield_single_thread_multiple_times, "1");
    qthread_run();

    void* value = qthread_join(thr);
    assert(!strcmp(value, "1"));

    qthread_t craete_2_thread[2] = {qthread_create(yield_single_thread_multiple_times, "1"),
                                    qthread_create(yield_single_thread_multiple_times, "2")};
    qthread_run();

    value = qthread_join(craete_2_thread[0]);
    assert(!strcmp(value, "1"));
    value = qthread_join(craete_2_thread[1]);
    assert(!strcmp(value, "2"));

    qthread_t create_3_threads[3] = {qthread_create(yield_single_thread_multiple_times, "1"),
                                     qthread_create(yield_single_thread_multiple_times, "2"),
                                     qthread_create(yield_single_thread_multiple_times, "3")};
    qthread_run();

    value = qthread_join(create_3_threads[0]);
    assert(!strcmp(value, "1"));
    value = qthread_join(create_3_threads[1]);
    assert(!strcmp(value, "2"));
    value = qthread_join(create_3_threads[2]);
    assert(!strcmp(value, "3"));
}

int in_mutex;

void* test_mutex_on_single_thread(void* arg) {
    // printf("Mutex test on a single thread \n");
    for (int i = 0; i < 3; i++) {
        qthread_mutex_lock(&m1);  //acquire lock on mutex
        assert(in_mutex == 0);
        in_mutex = 1;
        //printf("Mutex acquired by Thread %s \n", (char*) arg);
        qthread_usleep(10000);
        qthread_yield();
        //printf("Mutex left by thread %s \n", (char*) arg);
        assert(in_mutex == 1);
        in_mutex = 0;
        qthread_mutex_unlock(&m1);   // leave the lock on mutex
    }
    qthread_exit(arg);
}


void test_mutex_on_4_threads(void) {
    // printf("Mutex test for 4 threads");

    qthread_mutex_init(&m1);
    qthread_t threads[4] = {qthread_create(test_mutex_on_single_thread, "1"),
                            qthread_create(test_mutex_on_single_thread, "2"),
                            qthread_create(test_mutex_on_single_thread, "3"),
                            qthread_create(test_mutex_on_single_thread, "4")};
    qthread_run();
    return;
}

void* cond_var_test1(void* arg) {
    qthread_mutex_lock(&m1);   //obtain lock on mutex
    for (int c = 0; c < 3; c++) {
        if (c != 0) {
            qthread_cond_wait(&con1, &m1); // wait for cond1 on mutex m1
        } else
            qthread_mutex_unlock(&m1);   //release lock
        qthread_usleep(10000);
        qthread_cond_signal(&con2); // signal cond1 done after usleep
    }
    qthread_mutex_unlock(&m1);
    qthread_exit(arg);
}

void* cond_var_test2(void* arg) {
    qthread_mutex_lock(&m1);             // acquire lock on mutex
    for (int c = 0; c < 3; c++) {
        qthread_cond_wait(&con2, &m1);  //wait for cond 1
        qthread_usleep(10000);
        qthread_cond_signal(&con3);     // signal cond 2
        qthread_cond_wait(&con2, &m1);  // wait for cond2
        qthread_usleep(10000);
        qthread_cond_signal(&con1);     // signal cond1
    }
    qthread_mutex_unlock(&m1);    // release  lock on mutex
    qthread_exit(arg);
}

void cond_test_for_multiple_threads(void) {
    // printf("Testing the condition variables with multiple threads\n");
    qthread_mutex_init(&m1);
    qthread_cond_init(&con1);
    qthread_cond_init(&con2);
    qthread_cond_init(&con3);

    qthread_cond_signal(&con1);

    qthread_t threads[4] = {qthread_create(cond_var_test1, "1"),
                            qthread_create(cond_var_test2, "2"),
                            qthread_create(cond_var_test2, "3"),
                            qthread_create(cond_var_test1, "4")};
    qthread_run();
}

int cond_test_count = 0;

void cond_test() {
    qthread_usleep(100000);
    qthread_mutex_lock(&m1);     // acquire mutex   
    qthread_cond_wait(&con1, &m1);   // cond wait for con1
    qthread_usleep(100000);
    qthread_mutex_unlock(&m1);    // unlock the mutex
    cond_test_count++;
    qthread_exit(NULL);
}

void cond_var_broadcast_test() {
    qthread_usleep(100000);
    qthread_cond_broadcast(&con1);     //broadcast for con1
    qthread_usleep(100000);
    qthread_yield();
    qthread_exit(NULL);
}

void cond_variable_broadcast_test_on_multiple_threads(void) {
    //printf("Testing cond variables broadcast with 4 threads\n");
    qthread_mutex_init(&m1);
    qthread_cond_init(&con1);
    qthread_t threads[4] = {qthread_start(cond_test, "1", NULL),
                            qthread_start(cond_test, "2", NULL),
                            qthread_start(cond_test, "3", NULL),
                            qthread_start(cond_var_broadcast_test, "4", NULL)};
    qthread_run();
    assert(cond_test_count == 3);
    qthread_mutex_uninit(&m1);
    qthread_cond_uninit(&con1);
    return;
}


qthread_mutex_t filelock;

int has_written = 0;
int has_read = 0;

struct flock* fl;

void writer_thread() {
    fl = malloc(sizeof(struct flock));

    fl->l_len = 0;
    fl->l_type = F_WRLCK;
    fl->l_start = 0;
    fl->l_whence = SEEK_SET;

    int fd = open("test", O_CREAT | O_WRONLY);
    // int lockSucceed = fcntl(fd, F_SETLK, fl );
    qthread_mutex_lock(&filelock);

    qthread_yield();
    qthread_write(fd, "Hello", 6);
    has_written = 1;
    assert(!has_read);

    fl->l_type = F_UNLCK;
    // fcntl(fd, F_SETLK, fl);

    qthread_mutex_unlock(&filelock);

    close(fd);
    qthread_exit(NULL);
}

void reader_thread() {

    char* inStr = malloc(6);
    chmod("test", S_IRWXU); // For some reason the file gets created without read permission, this lets us read
    int fd = open("test", O_CREAT | O_RDONLY);

    assert(!has_written);

    qthread_mutex_lock(&filelock);

    qthread_read(fd, inStr, 6);

    qthread_mutex_unlock(&filelock);
    has_read = 1;
    assert(has_written);
    assert(inStr[0] == 'H');

    close(fd);
    qthread_exit(NULL);
}

void test_file_lock() {
    has_read = 0;
    has_written = 0;
    qthread_mutex_init(&filelock);
    remove("test");
    qthread_start(writer_thread, 0, 0);
    qthread_start(reader_thread, 0, 0);
    qthread_run();
    remove("test");
}

int pipefd[2];

void* pipe_writer() {
    qthread_yield();
    qthread_yield();
    assert(!has_read);
    qthread_write(pipefd[1], "Hello", 6);
    has_written = 1;
    qthread_yield();
    return NULL;
}

void* pipe_reader() {
    assert(!has_written);
    char first;
    qthread_read(pipefd[0], &first, 1);
    has_read = 1;
    assert(first == 'H');
    return NULL;
}

void test_file_lock_pipe() {
    has_read = 0;
    has_written = 0;
    pipe(pipefd);
    qthread_create(pipe_reader, NULL);
    qthread_create(pipe_writer, NULL);

    qthread_run();

    close(pipefd[0]);
    close(pipefd[1]);

    assert(has_written);
    assert(has_read);
}

void* get_random_number(void* num) {
    qthread_yield();
    qthread_yield();
    return (void*) (4 + (int) num);
}

void* joinExistingThread(void* qthread) {
    assert(5 == (int) qthread_join(qthread));
    return (void*) 1;
}

void do_join_tests() {
    qthread_t thr = qthread_create(get_random_number, (void*) 1);
    qthread_t secondthr = qthread_create(joinExistingThread, thr);
    assert(5 == (int) qthread_join(thr));
    assert(1 == (int) qthread_join(secondthr));
    qthread_exit(NULL);
}

void test_join_multiple() {
    qthread_start(do_join_tests, 0, 0);
    qthread_t thread_from_main = qthread_create(get_random_number, (void*) 6);
    qthread_run();

    qthread_t thread_from_main_2 = qthread_create(get_random_number, (void*) 7);
    assert(10 == (int) qthread_join(thread_from_main));
    assert(11 == (int) qthread_join(thread_from_main_2));

}

unsigned start_sleep_time;

#define assert_time_about(time, range) \
    /* printf("expected time: %d, actual time: %d\n", time, get_usecs()-start_sleep_time); */ \
    assert(get_usecs() >= (start_sleep_time + time) && get_usecs() < (start_sleep_time + time + range));


void sleeper_thread_1() {
    assert_time_about(0, 10000);

    qthread_yield();
    qthread_usleep(500000);
    qthread_yield();
    assert_time_about(500000, 500000);

    qthread_yield();
    qthread_usleep(500000);
    qthread_yield();
    assert_time_about(1000000, 500000);

    qthread_exit(0);
}

int has_accessed_resource = 0;

void sleeper_thread_holding_mutex() {
    qthread_mutex_lock(&m1);
    qthread_yield();
    qthread_usleep(100000);
    qthread_yield();
    assert(!has_accessed_resource);
    qthread_mutex_unlock(&m1);
    qthread_exit(NULL);
}

void sleeper_thread_access_resource() {
    qthread_mutex_lock(&m1);
    has_accessed_resource = 1;
    qthread_mutex_unlock(&m1);
    qthread_exit(NULL);
}

void test_sleep_timing() {
    start_sleep_time = get_usecs();

    qthread_start(sleeper_thread_1, 0, 0);
    qthread_start(sleeper_thread_1, 0, 0);
    qthread_run();
    assert_time_about(1000000, 500000);

    qthread_mutex_init(&m1);
    qthread_start(sleeper_thread_holding_mutex, 0, 0);
    qthread_start(sleeper_thread_access_resource, 0, 0);
    qthread_run();
    assert(has_accessed_resource);
    qthread_mutex_uninit(&m1);
}

int main(int argc, char** argv) {
    // printf("Hello.\n");
    // qthread_create(test1, 0);
    // qthread_create(test2, 0);

    yield_multiple_threads_with_join();
    test_mutex_on_4_threads();
    cond_test_for_multiple_threads();
    cond_variable_broadcast_test_on_multiple_threads();

    test_file_lock();
    test_file_lock_pipe();
    test_join_multiple();
    test_sleep_timing();

    return 0;
}
