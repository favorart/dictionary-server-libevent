#include "header.h"
#include "srv.h"


void  find_words_prefix (struct evbuffer *buf_inc,
                         struct evbuffer *buf_out);
//-----------------------------------------
void  server_job_function (job_t *job)
{
  struct client *Client = (struct client*) job->user_data;
  //----------------------------------------------------------------------
  event_base_loop (Client->base, 0);
  client_free (Client);
  free (job);
}
//-----------------------------------------
void  client_free (struct client *Client)
{
  bufferevent_free (Client->b_ev);
  event_base_free  (Client->base);
  free (Client);
}
//-----------------------------------------
void  srv_ac_err_cb (evutil_socket_t fd, short ev, void *arg)
{
  struct server_config *server_conf = (struct server_config*) arg;
  //----------------------------------------------------------------------
  int err = EVUTIL_SOCKET_ERROR ();
  fprintf (stderr, "Got an error %d (%s) on the listener. "
           "Shutting down.\n", err, evutil_socket_error_to_string (err));
  //----------------------------------------------------------------------
  event_base_loopexit (server_conf->base, NULL);
}
void  srv_accept_cb (evutil_socket_t fd, short ev, void *arg)
{
  struct server_config *server_conf = (struct server_config*) arg;
  //----------------------------------------------------------------------
  int  SlaveSocket = accept (fd, 0, 0);
  if ( SlaveSocket == -1 )
  { fprintf (stderr, "%s\n", strerror (errno));
    return;
  }
  //----------------------------------------------------------------------
  set_nonblock (SlaveSocket);
  //----------------------------------------------------------------------
  /* Making the new client */
  struct client *Client = (struct client*) calloc (1U, sizeof (*Client));
  if ( !Client )
  { perror ("client calloc");
    return;
  }

  if ( !(Client->base = event_base_new ()) )
  {
    my_errno = SRV_ERR_LIBEV;
    fprintf (stderr, "%s\n", strmyerror ());

    client_free (Client);
    return;
  }
  //----------------------------------------------------------------------
  /* Create new bufferized event, linked with client's socket */
  if ( !(Client->b_ev = bufferevent_socket_new (Client->base, SlaveSocket, BEV_OPT_CLOSE_ON_FREE)) )
  {
    my_errno = SRV_ERR_LIBEV;
    fprintf (stderr, "%s\n", strmyerror ());

    client_free (Client);
    return;
  }
  bufferevent_setcb (Client->b_ev, srv_read_cb, NULL, srv_error_cb, Client);
  /* Ready to get data */
  bufferevent_enable (Client->b_ev, EV_READ | EV_WRITE | EV_PERSIST);
  //----------------------------------------------------------------------
#ifdef _DEBUG
  printf ("connection to server\n");
#endif // _DEBUG

  job_t  *job = NULL;
  /* Create a job object and add it to the work queue. */
  if ( !(job = malloc (sizeof (*job))) )
  {
    perror ("job malloc");
    client_free (Client);
    return;
  }

  job->job_func  = server_job_function;
  job->user_data = Client;

  wque_push (&server_conf->workqueue, job);
  //----------------------------------------------------------------------
#ifdef _DEBUG
  printf ("connection accepted\n");
#endif // _DEBUG
  //----------------------------------------------------------------------
}
//-----------------------------------------
void  srv_error_cb (struct bufferevent *b_ev, short events, void *arg)
{
  struct client  *Client = (struct client*) arg;
  //----------------------------------------------------------------------
#ifdef _DEBUG
  if ( events & BEV_EVENT_CONNECTED )
  { printf ("connection established\n");
    return;
  }
#endif // _DEBUG  
  if ( events & BEV_EVENT_EOF )
  {
#ifdef _DEBUG
    printf ("got a close. length = %u\n", evbuffer_get_length (bufferevent_get_input (Client->b_ev)));
#endif // _DEBUG
    shutdown (bufferevent_getfd (Client->b_ev), SHUT_RDWR);
  }
  //----------------------------------------------------------------------
  if ( events & BEV_EVENT_ERROR )
  { fprintf (stderr, "Error from bufferevent: '%s'\n",
             evutil_socket_error_to_string (EVUTIL_SOCKET_ERROR ()));
  }
  //----------------------------------------------------------------------
  // client_free (Client);
#ifdef _DEBUG
  printf ("connection closed");
#endif // _DEBUG
  //----------------------------------------------------------------------
}
void  srv_read_cb  (struct bufferevent *b_ev, void *arg)
{
#ifdef _DEBUG
  printf ("data reseived\n");
#endif // _DEBUG
  //----------------------------------------------------------------------
  /* This callback is invoked when there is data to read on b_ev_read */
  struct client   *Client = (struct client*) arg;

  struct evbuffer *buf_in  = bufferevent_get_input  (b_ev);
  struct evbuffer *buf_out = bufferevent_get_output (b_ev);

  /* Copy all the data from the input buffer to the output buffer. */
  find_words_prefix (buf_in, buf_out);
  //----------------------------------------------------------------------
#ifdef _DEBUG
  printf ("response ready\n");
#endif // _DEBUG
}
//-----------------------------------------
#define WORD_LEN 101
#define WORD_CNT 10

void  find_words_prefix (struct evbuffer *buf_inc,
                         struct evbuffer *buf_out)
{
  char      words[WORD_CNT][WORD_LEN];
  size_t  weights[WORD_CNT];
  char   *prefix = NULL;
  size_t  prefix_length = 0U;
  //-----------------------------------------------------------------
  FILE *fdict = fopen (SRV_DICT_FLNAME, "r");
  if ( !fdict )
  {
    perror ("dict file");
    goto HNDL_FREE;
  }

  while ( (prefix = evbuffer_readln (buf_inc, &prefix_length, EVBUFFER_EOL_ANY)) )
  {
    int     i = 0;
    char    word[WORD_LEN];
    size_t  weight = 0;

#ifdef  _DEBUG
    printf ("\n\n--> %s\n", prefix);
#endif // _DEBUG
    //----------------------------------------------------------------------
    while ( 2 == fscanf (fdict, "%s %d", word, &weight) )
    {

      if ( !strncmp (word, prefix, prefix_length) )
      {
        /* sorted_by weight */
        if ( i >= WORD_CNT )
        {
          for ( int j = 0; j < WORD_CNT; ++j )
          {
            if ( weight > weights[j] )
            {
              strcpy (words[j], word);
              weights[j] = weight;
              break;
            }
          }
        }
        else
        {
          strcpy (words[i], word);
          weights[i] = weight;
          ++i;
        } // else
      }
    }
    //-----------------------------------------------------------------
    if ( buf_out )
    {
      for ( int j = 0; j < i; ++j )
      {
#ifdef  _DEBUG
        printf ("%s %d\n", words[j], weights[j]);
#endif // _DEBUG

        evbuffer_add_printf (buf_out, "%s\n", words[j]);
      }
    }

    free (prefix);
    continue;

REQ_ERR:;
    my_errno = SRV_ERR_RCMMN;
    fprintf (stderr, "%s", strmyerror ());
    free (prefix);
  }
  //-----------------------------------------------------------------
HNDL_FREE:;
  free (prefix);
  fclose (fdict);
  evbuffer_drain (buf_inc, -1);
  //----------------------------------------------------------------------
}
//-----------------------------------------
