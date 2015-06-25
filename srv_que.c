#include "header.h"
#include "srv_que.h"

//-----------------------------------------
static void*  worker_function (void *ptr)
{
  worker_t *worker = (worker_t*) ptr;

  while ( true )
  {
    /* Wait until we get notified. */
    pthread_mutex_lock (&worker->wque->jobs_mutex);
    while ( worker->wque->waiting_jobs == NULL )
    {
      /* If we're supposed to terminate, break out of our continuous loop. */
      if ( worker->term ) break;

      pthread_cond_wait (&worker->wque->jobs_cond, &worker->wque->jobs_mutex);
    }

    /* If we're supposed to terminate, break out of our continuous loop */
    if ( worker->term ) break;

    job_t *job = worker->wque->waiting_jobs;
    if ( job )
    { LL_DEL (job, worker->wque->waiting_jobs); }
    pthread_mutex_unlock (&worker->wque->jobs_mutex);

    /* If job is got */
    if ( job )
    {
      /* Execute the job. */
      job->job_func (job);
    }
  }

  free (worker);
  pthread_exit (NULL);

  return NULL;
}
//-----------------------------------------
int   wque_init (wque_t *wq, int n_workers)
{
  worker_t *worker;
  pthread_cond_t  blank_cond  = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;

  if ( n_workers < 1 )
  { n_workers = 1; }

  memset (wq, 0, sizeof (*wq));
  memcpy (&wq->jobs_mutex, &blank_mutex, sizeof (wq->jobs_mutex));
  memcpy (&wq->jobs_cond,  &blank_cond,  sizeof (wq->jobs_cond));

  for ( int i = 0; i < n_workers; i++ )
  {
    if ( !(worker = malloc (sizeof (worker_t))) )
    { perror ("failed to allocate all workers");
      return -1;
    }

    memset (worker, 0, sizeof (*worker));
    worker->wque = wq;
    
    if ( pthread_create (&worker->thread, NULL, worker_function, (void*) worker) )
    { perror ("failed to start all worker threads");
      free (worker);
      return -1;
    }

    LL_ADD (worker, worker->wque->workers);
  }

  return 0;
}
void  wque_free (wque_t *wq)
{
  /* Set all workers to terminate. */
  for ( worker_t *worker = wq->workers; worker; worker = worker->next )
  { worker->term = true; }

  /* Remove  all workers and jobs from the work queue.
   * wake up all workers so that they will terminate. */
  pthread_mutex_lock (&wq->jobs_mutex);

  wq->workers = NULL;
  wq->waiting_jobs = NULL;

  pthread_cond_broadcast (&wq->jobs_cond);
  pthread_mutex_unlock   (&wq->jobs_mutex);
}
void  wque_push (wque_t *wq, job_t *job)
{
  /* Add a job to the queue and notify a worker */
  pthread_mutex_lock (&wq->jobs_mutex);

  LL_ADD (job, wq->waiting_jobs);

  pthread_cond_signal  (&wq->jobs_cond);
  pthread_mutex_unlock (&wq->jobs_mutex);
}
//-----------------------------------------
