typedef struct thread_pool_s {
	int work_queue_id;
} thread_pool_t;

typedef struct work_order_s {
	struct timespec *timelimit;
	void *job_ctx;
	void *result;
} work_order_t;

typedef struct pool_ctx_s {
	pthread_mutex_t *mx;
	void *(*routine)(void *);
	thread_pool_t *info; 	/* public info */
	int count_thread;	/* number of threads in the pool */
	int count_idle;		/* idling threads */
} pool_ctx_t;
