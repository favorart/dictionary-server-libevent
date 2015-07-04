#include "header.h"
#include "srv.h"

#ifndef _CACHE_HASH_H_
#define _CACHE_HASH_H_
//-----------------------------------------
typedef  uint16_t hash_t;
// typedef   int32_t ht_key_t;
// typedef   int32_t ht_val_t;

typedef struct hashtable_record ht_rec;
struct hashtable_record
{
  ht_key_t  key;
  ht_val_t  val;
    time_t  ttl;
};
//-----------------------------------------
typedef struct hashtable_line ht_line;
struct hashtable_line
{
  bool    busy;
  ht_rec  data;
};
//-----------------------------------------
typedef struct hash_table hashtable;
struct hash_table
{
  char         *shmemory;
  size_t        shm_size;
  size_t     lines_count;
  ht_line         *lines;
  //------------------------
  pthread_mutex_t  mutex;
  //------------------------
  int (*key_comp) (ht_key_t *a, ht_key_t *b);
};
//-----------------------------------------
time_t  ttl_converted (int32_t ttl);
//-----------------------------------------
void  hashtable_init (hashtable *ht, size_t lines_count, size_t size, int (*key_comp) (ht_key_t *a, ht_key_t *b));
void  hashtable_free (hashtable *ht);
bool  hashtable_get  (hashtable *ht, ht_rec *data);
bool  hashtable_set  (hashtable *ht, ht_rec *data);
//-----------------------------------------
#ifdef  _DEBUG_HASH
void  hashtable_print_debug (hashtable *ht, FILE *f);
#endif // _DEBUG_HASH
//-----------------------------------------
#endif // _CACHE_HASH_H_
