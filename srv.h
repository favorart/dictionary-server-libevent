#include "header.h"
#include "srv_que.h"

/*  Практика по потокам (pthreads, std::threads, boost::threads)
 *  ! LINUX АРХИТЕКТУРА !
 *
 *  v   Сборка через make.
 *  v   Запуск:  ./dict ...
 *  v   Сервер при старте создаёт 4 потока - обработки комманд клиентов.

 *  v  Tекстовый файл со словами (за каждым словом через пробел лежит int его вес (0-99)) - общая база данных.
 *  v  В файле линейным поиском надо искать слова с переданным по сети префиксом.
 *  v  Ответы выдавать в порядке убывания веса, но не более 10ти.
 *  v  1 соединение - 1 запрос.
 *
 *  v  Один поток получает соединения - остальные обрабатывают (загружаются равномерно).
 *  v  Между ними mutexes и conditional_variables.
 *
 *  -  Также есть cache, в котором хранятся последние слова, к которым обращались.
 *  -  У кэша есть TTL, и он общий (read/write lock)
 */
#ifndef _SRV_DICT_H_
#define _SRV_DICT_H_

#define _DEBUG
//-----------------------------------------
typedef enum
{ SRV_ERR_NONE,  SRV_ERR_PARAM,
  SRV_ERR_INPUT, SRV_ERR_LIBEV,
  SRV_ERR_RCMMN, SRV_ERR_RFREE,
  SRV_ERR_FDTRS
} myerr;
myerr my_errno;
const char*  strmyerror ();
//-----------------------------------------
#define  SRV_SERVER_NAME   "dict-server-pract6"
#define  SRV_IPSTRLENGTH   16U
#define  SRV_MAX_WORKERS   16U
#define  SRV_BUF_LOWMARK  128U
#define  SRV_DICT_FLNAME   "mydict.txt"

typedef uint8_t wc_t;
typedef struct hash_table    hashtable;
typedef struct server_config  srv_conf;
struct  server_config
{
  //-----------------------
  char *ptr_server_name;
  //-----------------------
  uint16_t  port;
  char      ip[SRV_IPSTRLENGTH];
  //-----------------------
  /* defines FILENAME_MAX in <stdio.h> */
  char  config_path[FILENAME_MAX];
  char  server_path[FILENAME_MAX];
  //-----------------------
  wc_t       workers_count;
  worker_t  *workers;
  wque_t     workqueue;
  //-----------------------
  struct event_base  *base;
  struct hash_table  *ht;
};
extern  struct server_config  server_conf;
//-----------------------------------------
int     server_config_init  (srv_conf *conf, char *port, char *ip, char *work);
void    server_config_print (srv_conf *conf, FILE *stream);
void    server_config_free  (srv_conf *conf);
//-----------------------------------------
int     parse_console_parameters (int argc, char **argv, srv_conf *conf);
//-----------------------------------------
int     set_nonblock (evutil_socket_t fd);
//-----------------------------------------
struct  client
{
  /* bufferevent already has separate
   * two buffers for input and output.
   */
  struct bufferevent  *b_ev;
  struct event_base   *base;
  //---------------------------
  struct hash_table   *ht;
};
void    client_free (struct client *Client);
//-----------------------------------------
void    srv_accept_cb (evutil_socket_t fd, short ev, void *arg);
void    srv_ac_err_cb (evutil_socket_t fd, short ev, void *arg);
//-----------------------------------------
void    srv_read_cb  (struct bufferevent *b_ev, void *arg);
void    srv_error_cb (struct bufferevent *b_ev, short events, void *arg);
//-----------------------------------------
#define DICT_CACHELNS 256
#define DICT_WORD_LEN 101
#define DICT_WORD_CNT 10
struct dict_rec
{ char   words[DICT_WORD_CNT][DICT_WORD_LEN];
  int  weights[DICT_WORD_CNT];
  int  count;
};
struct  key_rec
{ char word[DICT_WORD_LEN]; };

typedef struct dict_rec ht_val_t;
typedef struct  key_rec ht_key_t;
static inline int  my_key_comp (ht_key_t *a, ht_key_t* b)
{ return strcmp (a->word, b->word); }
//-----------------------------------------
#endif // _SRV_DICT_H_
