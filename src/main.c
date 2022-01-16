#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <malloc.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>

typedef void (*task_t) (void *args);

typedef struct work{
    task_t task;
    struct work* next;
}work_t;

typedef struct threadpool{
    size_t num_threads;
    pthread_t* threads;
    work_t* work_first;
    work_t* work_last;
    pthread_mutex_t mutex;
    pthread_cond_t wcond;
    bool stop;
}threadpool_t;

bool add_work(threadpool_t *tp, task_t func){
    work_t* w;
    w = malloc(sizeof(work_t));

    if(func == NULL) return false;
    w->task = func;

    pthread_mutex_lock(&(tp->mutex));
    if(tp->work_first == NULL){
        tp->work_first = w;
        tp->work_last = w;
    }
    else{
        tp->work_last->next = w;
        tp->work_last = w;
    }


    pthread_mutex_unlock(&(tp->mutex));
    pthread_cond_broadcast(&(tp->wcond));
    return true;
}

work_t* get_work(threadpool_t* tp){
    if(tp == NULL) return NULL;
    if(tp->work_first == NULL) return NULL;

    work_t* w = tp->work_first;
    tp->work_first = tp->work_first->next;
    if(tp->work_first == NULL){
        tp->work_last = NULL;
    }

    return w;
}

void free_work(work_t* w){
    if(w == NULL) return;
    free(w);
}

void* worker(void* args){
    threadpool_t* tp = (threadpool_t*) args;
    while(1){
        pthread_mutex_lock(&(tp->mutex));
        while(tp->work_first == NULL && !(tp->stop)){
            pthread_cond_wait( &(tp->wcond), &(tp->mutex));
        }

        if(tp->stop) break;

        work_t *w = get_work(tp);
        pthread_mutex_unlock(&(tp->mutex));
        w->task(NULL);
        free_work(w);
    }
    return NULL;
}

threadpool_t* create_threadpool(size_t num){
    pthread_t *thread = malloc(sizeof(pthread_t)*num );
    threadpool_t *tp = malloc(sizeof (threadpool_t));
    tp->num_threads = num;
    tp->threads = thread;

    pthread_mutex_init(&(tp->mutex), NULL);
    pthread_cond_init(&(tp->wcond), NULL);

    //start threads for that specific thread pool
    for(int i = 0; i<num;i++){
        pthread_create(&thread[i], NULL, worker, tp);
    }
    return tp;
}

void threadpool_wait(threadpool_t* tp){
    for(int i = 0;i<tp->num_threads;i++){
        pthread_join(tp->threads[i], NULL);
    }
}

void destroy_threadpool(threadpool_t* tp){
    work_t* w1;
    work_t* w2;
    pthread_mutex_lock(&(tp->mutex));
    w1 = tp->work_first;
    while(w1 != NULL){
        w2 = w1->next;
        free_work(w1);
        w1 = w2;
    }
    tp->stop = true;
    pthread_mutex_unlock(&(tp->mutex));
    pthread_cond_broadcast(&(tp->wcond));
    threadpool_wait(tp);
    free(tp);
}

void testFunc(void* args){
    printf("test task!\n");
}

int main() {
    threadpool_t* tp = create_threadpool(1);
    add_work(tp, testFunc);
    add_work(tp, testFunc);
    add_work(tp, testFunc);
    
    sleep(2);
    destroy_threadpool(tp);
    printf("threadpool destroyed!\n");
    //threadpool_wait(tp);
    return 0;
}