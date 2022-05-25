#include "thread_pool.h"

/*创建线程池*/
thread_pool_t *create_thread_pool(int work_thread_num_min, int work_thread_num_max, int queue_capacity)
{
	printf("begin to create thread pool\n");
	thread_pool_t *pool = calloc(1,sizeof(thread_pool_t));
	do{
		if (pool == NULL){
			printf("calloc thread_pool fail...\n");
			break;
		}

		pool->thread_ids = calloc(work_thread_num_max,sizeof(pthread_t));
		if (pool->thread_ids == NULL)
		{
			printf("calloc thread_ids fail...\n");
			break;
		}

		pool->min_num = work_thread_num_min;
		pool->max_num = work_thread_num_max;
		pool->busy_num = 0;
		pool->live_num = work_thread_num_min; // 和最小个数相等
		pool->exit_num = 0;

		if (pthread_mutex_init(&pool->mutex_pool, NULL) != 0 ||
			pthread_mutex_init(&pool->mutex_busy, NULL) != 0 ||
			pthread_cond_init(&pool->is_empty, NULL) != 0 ||
			pthread_cond_init(&pool->is_full, NULL) != 0)
		{
			printf("mutex or condition init fail...\n");
			break;
		}

		// 任务队列
		pool->queue_capacity = queue_capacity;
		pool->task_queue = calloc(pool->queue_capacity,sizeof(task_t));
		pool->queue_curr_size = 0;
		pool->queue_front = 0;
		pool->queue_rear = 0;

		pool->shutdown = 0;

		// 创建线程
		printf("begin to create manager thread.\n");
		pthread_create(&pool->manager_id, NULL, manager, pool);
		printf("create manager thread[%lu] successful.\n",pool->manager_id);

		for (int i = 0; i < work_thread_num_min; ++i){
			printf("begin to create worker thread.\n");
			pthread_create(&pool->thread_ids[i], NULL, worker, pool);
			printf("create worker[%lu] thread successful.\n",pool->thread_ids[i]);
		}

		printf("create thread pool sucessful work_thread_num_min[%d] work_thread_num_max[%d] queue_capacity[%d].\n",
			    work_thread_num_min,work_thread_num_max,queue_capacity);
		return pool;
	} while (0);

	// 释放资源
	if (pool && pool->thread_ids)
		free(pool->thread_ids);
	if (pool && pool->task_queue)
		free(pool->task_queue);
	if (pool)
		free(pool);

	return NULL;
}

/*销毁线程池*/
int destroy_thread_pool(thread_pool_t *pool)
{
	if (pool == NULL){
		return -1;
	}
	printf("begin to destroy thread pool.\n");
	// 关闭线程池
	pthread_mutex_lock(&pool->mutex_pool);
	pool->shutdown = 1;
	pthread_mutex_unlock(&pool->mutex_pool);
	// 阻塞回收管理者线程
	pthread_join(pool->manager_id, NULL);
	// 唤醒阻塞的消费者线程
	for (int i = 0; i < pool->live_num; ++i){
		pthread_cond_signal(&pool->is_empty);
	}

	// 释放堆内存
	if (pool->task_queue){
		free(pool->task_queue);
		pool->task_queue = NULL;
	}
	if (pool->thread_ids){
		free(pool->thread_ids);
		pool->thread_ids = NULL;
	}

	pthread_mutex_destroy(&pool->mutex_pool);
	pthread_mutex_destroy(&pool->mutex_busy);
	pthread_cond_destroy(&pool->is_empty);
	pthread_cond_destroy(&pool->is_full);

	free(pool);
	pool = NULL;
	printf("destroy thread pool sucessful.\n");
	return 0;
}

/*生产者，往线程池添加任务*/
void add_task_to_thread_pool(thread_pool_t *pool, task_t *task)
{
	pthread_mutex_lock(&pool->mutex_pool);
	printf("begin to add task[arg:%d] to thread pool.\n",*(int*)task->arg);
	while (pool->queue_curr_size == pool->queue_capacity && !pool->shutdown){ //线程池的队列满了，无法继续添加任务了

		/*pthread_cond_wait用于阻塞当前线程，等待别的线程使用pthread_cond_signal或pthread_cond_broadcast来唤醒它。 
		pthread_cond_wait必须与pthread_mutex 配套使用。pthread_cond_wait函数一进入wait状态就会自动release mutex。
		当其他线程通过pthread_cond_signal或pthread_cond_broadcast，把该线程唤醒，使pthread_cond_wait返回时，
		该线程又自动获得该mutex。*/
		pthread_cond_wait(&pool->is_full, &pool->mutex_pool); //当前阻塞生产者
	}

	if (pool->shutdown){
		printf("thread pool is shutdown,add task to thread pool fail...\n");
		pthread_mutex_unlock(&pool->mutex_pool);
		return;
	}

	// 添加任务
	pool->task_queue[pool->queue_rear].function = task->function;
	pool->task_queue[pool->queue_rear].arg = task->arg;
	pool->queue_rear = (pool->queue_rear + 1) % pool->queue_capacity;
	pool->queue_curr_size++;

	/*pthread_cond_signal函数的作用是发送一个信号给另外一个正在处于阻塞等待状态的线程,
	  使其脱离阻塞状态,继续执行.如果没有线程处在阻塞等待状态,pthread_cond_signal也会成功返回*/

	pthread_cond_signal(&pool->is_empty); //唤醒消费者线程
	printf("add task to thread pool successful.\n");
	pthread_mutex_unlock(&pool->mutex_pool);
}

/*获取线程池中忙的线程的个数*/
int get_thread_pool_busy_num(thread_pool_t *pool)
{
	pthread_mutex_lock(&pool->mutex_busy);
	int busy_num = pool->busy_num;
	pthread_mutex_unlock(&pool->mutex_busy);

	return busy_num;
}

/*获取线程池中存活的线程的个数*/
int get_thread_pool_alive_num(thread_pool_t *pool)
{
	pthread_mutex_lock(&pool->mutex_pool);
	int alive_num = pool->live_num;
	pthread_mutex_unlock(&pool->mutex_pool);

	return alive_num;
}

/*工作者线程函数，有多个工作者线程*/
void *worker(void *arg)
{
	thread_pool_t *pool = (thread_pool_t *)arg;
	while (1){
		pthread_mutex_lock(&pool->mutex_pool);
		// 当前任务队列是否为空
		while (pool->queue_curr_size == 0 && !pool->shutdown){ //没有任务可做，线程处于空闲状态
			pthread_cond_wait(&pool->is_empty, &pool->mutex_pool); //阻塞工作线程
			// 判断是不是要销毁线程
			if (pool->exit_num > 0){
				pool->exit_num--;
				if (pool->live_num > pool->min_num){
					pool->live_num--;
					pthread_mutex_unlock(&pool->mutex_pool);
					thread_exit(pool);
				}
			}
		}

		// 判断线程池是否被关闭了
		if (pool->shutdown){
			pthread_mutex_unlock(&pool->mutex_pool);
			thread_exit(pool);
		}

		// 从任务队列中取出一个任务
		task_t task;
		task.function = pool->task_queue[pool->queue_front].function;
		task.arg = pool->task_queue[pool->queue_front].arg;

		// 移动头结点
		pool->queue_front = (pool->queue_front + 1) % pool->queue_capacity;
		pool->queue_curr_size--;

		// 解锁
		pthread_cond_signal(&pool->is_full);
		pthread_mutex_unlock(&pool->mutex_pool);

		printf("thread %ld start working...\n", pthread_self());

		pthread_mutex_lock(&pool->mutex_busy);
		pool->busy_num++;
		pthread_mutex_unlock(&pool->mutex_busy);

		task.function(task.arg);//执行任务

		free(task.arg);
		task.arg = NULL;

		printf("thread %ld end working...\n", pthread_self());

		pthread_mutex_lock(&pool->mutex_busy);
		pool->busy_num--;
		pthread_mutex_unlock(&pool->mutex_busy);
	}

	return NULL;
}

/*管理线程函数，只有一个管理线程*/
void *manager(void *arg)
{
	thread_pool_t *pool = (thread_pool_t *)arg;
	while (1){
		sleep(1);// 每隔3s检测一次
		printf("do manager thread.\n");
		pthread_mutex_lock(&pool->mutex_pool);
		if(!pool->shutdown){
			pthread_mutex_unlock(&pool->mutex_pool);
			break;
		}

		// 取出忙的线程的数量
		pthread_mutex_lock(&pool->mutex_busy);
		int busy_num = pool->busy_num;
		pthread_mutex_unlock(&pool->mutex_busy);

		// 添加线程
		// 任务的个数>存活的线程个数 && 存活的线程数<最大线程数
		if (pool->queue_curr_size > pool->live_num && pool->live_num < pool->max_num){
			int counter = 0;
			for (int i = 0; i < pool->max_num && counter < NUMBER && pool->live_num < pool->max_num; ++i){
				if (pool->thread_ids[i] == 0){
					pthread_create(&pool->thread_ids[i], NULL, worker, pool);
					counter++;
					pool->live_num++;
				}
			}
		}
		// 销毁线程
		// 忙的线程*2 < 存活的线程数 && 存活的线程>最小线程数
		if (busy_num * 2 < pool->live_num && pool->live_num > pool->min_num){
			pool->exit_num = NUMBER;
			// 让工作的线程自杀
			for (int i = 0; i < NUMBER; ++i){
				pthread_cond_signal(&pool->is_empty);
			}
		}
		pthread_mutex_unlock(&pool->mutex_pool);
	}
	return NULL;
}

/*找到当前线程，主动退出*/
void thread_exit(thread_pool_t *pool)
{
	pthread_t tid = pthread_self();
	printf("thread [%lu] begin to exit!\n",tid);
	for (int i = 0; pool && pool->thread_ids && i < pool->max_num; ++i){
		if (pool->thread_ids[i] == tid){
			pool->thread_ids[i] = 0;
			printf("thread_exit() called, %ld exiting...\n", tid);
			break;
		}
	}
	printf("thread [%lu] exit sucessful!\n",tid);
	pthread_exit(NULL);
}
