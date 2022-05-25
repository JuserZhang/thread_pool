#include "thread_pool.h"

void task_func(void *arg)
{
	int num = *(int *)arg;
	printf("thread %ld is working, number = %d\n",
		   pthread_self(), num);
	sleep(1);
}

int main()
{
	// 创建线程池
	thread_pool_t *pool = create_thread_pool(5, 10, 100);
	for (int i = 0; i < 50; ++i)
	{
		task_t task;
		int *arg = calloc(1,sizeof(int*));
		*arg = i;

		task.function = task_func;
		task.arg = (void*)arg;

		add_task_to_thread_pool(pool,&task);
	}

	sleep(30);

	destroy_thread_pool(pool);
	return 0;
}
