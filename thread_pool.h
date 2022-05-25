#ifndef _THREAD_POOL_H
#define _THREAD_POOL_H

#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <pthread.h>

#define NUMBER 1

// 任务结构体
typedef struct task
{
	void (*function)(void *arg);
	void *arg;
} task_t;

// 线程池结构体
typedef struct thread_pool
{
	task_t *task_queue;	// 任务队列
	int queue_capacity; // 队列容量（能放入任务的最大数量）
	int queue_curr_size;// 当前任务个数
	int queue_front;	// 队头 -> 取数据
	int queue_rear;		// 队尾 -> 放数据

	pthread_t manager_id;		// 管理者线程ID
	pthread_t *thread_ids;		// 工作的线程ID
	int min_num;				// 最小线程数量
	int max_num;				// 最大线程数量
	int busy_num;				// 忙的线程的个数
	int live_num;				// 存活的线程的个数
	int exit_num;				// 要销毁的线程个数
	pthread_mutex_t mutex_pool;	// 锁整个的线程池
	pthread_mutex_t mutex_busy;	// 锁busyNum变量
	pthread_cond_t is_full;		// 任务队列是满了
	pthread_cond_t is_empty;	// 任务队列空了

	int shutdown;	//是不是要销毁线程池, 销毁为1, 不销毁为0
} thread_pool_t;

// 创建线程池并初始化
thread_pool_t *create_thread_pool(int min, int max, int queue_size);

// 销毁线程池
int destroy_thread_pool(thread_pool_t *pool);

// 给线程池添加任务
void add_task_to_thread_pool(thread_pool_t *pool, task_t *task);

// 获取线程池中工作的线程的个数
int get_thread_pool_busy_num(thread_pool_t *pool);

// 获取线程池中活着的线程的个数
int get_thread_pool_alive_num(thread_pool_t *pool);

// 工作的线程(消费者线程)任务函数
void *worker(void *arg);

// 管理者线程任务函数
void *manager(void *arg);

// 单个线程退出
void thread_exit(thread_pool_t *pool);
#endif // _THREADPOOL_H
