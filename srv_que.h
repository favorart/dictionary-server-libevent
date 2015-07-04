#include "header.h"

#ifndef _WORKQUEUE_H_
#define _WORKQUEUE_H_
//-----------------------------------------
typedef struct worker worker_t;
struct worker
{
  //-----------------------
  pthread_t        thread;
  bool               term;
  struct workqueue  *wque;
  //-----------------------
  struct worker     *prev;
  struct worker     *next;
 };
//-----------------------------------------
typedef struct job job_t;
struct job
{
  //--------------------
  void  (*job_func)(struct job *job);
  void  *user_data;
  //--------------------
  struct job *prev;
  struct job *next;
};

void  server_job_function (job_t *job);
//-----------------------------------------
typedef struct workqueue wque_t;
struct workqueue
{
  struct worker *workers;
  struct job    *waiting_jobs;
  //--------------------
  pthread_mutex_t jobs_mutex;
  pthread_cond_t  jobs_cond;
};

int   wque_init (wque_t *wq, int n_workers);
void  wque_free (wque_t *wq);
void  wque_push (wque_t *wq, job_t *job);

//-----------------------------------------
#define LL_ADD(item, list) { \
	item->prev = NULL; \
	item->next = list; \
	list = item; \
}
#define LL_DEL(item, list) { \
	if (item->prev != NULL) item->prev->next = item->next; \
	if (item->next != NULL) item->next->prev = item->prev; \
	if (list == item) list = item->next; \
	item->prev = item->next = NULL; \
}
//-----------------------------------------
#endif //	_WORKQUEUE_H_
