#include "header.h"
#include "srv_cache.h"
#include "srv.h"

//-----------------------------------------
#define TIME_EPS 0.01
/* Relative TTL to absolute TTL (in seconds) */
time_t  ttl_converted (int32_t ttl)
{ return (time (NULL) + ttl); }
bool    ttl_completed (time_t  ttl)
{ return (difftime (time (NULL), ttl) > TIME_EPS); }
//-----------------------------------------
void  hashtable_init (hashtable *ht, size_t lines_count, size_t size)
{
  //------------------------------------------
  ht->lines_count = lines_count;
  if ( sizeof (ht_line) * lines_count > size )
  { perror ("ht size");
    ht->shm_size = lines_count * sizeof (ht_line);
  }
  else
  { ht->shm_size = size; }
  //-----------------------------------------
#ifdef _DEBUG
  static char mem[CACHELINES * sizeof (ht_line)] = { 0 };
  ht->shmemory = mem;
  ht->lines = (ht_line*) ht->shmemory;
#endif // _DEBUG
#ifdef _LOCK
  mysem_val_t val = 1;
  /* Создание одного семафора, проинициализированного */
  ht->tb_semid = mysem_create (1U, &val);

  /* Создание общей памяти */
  ht->tb_shmid = myshm_create (ht->shm_size);
  ht->shmemory = myshm_append (ht->tb_shmid);

  /* Инициализация общей памяти */
  memset (ht->shmemory, 0, shm_size);
#endif // _LOCK
  //-----------------------------------------
  return;
}
void  hashtable_free (hashtable *ht)
{
#ifdef _LOCK
  myshm_remove (ht->tb_shmid);
  mysem_remove (ht->tb_semid);
#endif // _LOCK
}
//-----------------------------------------
static hash_t  hash (ht_key_t key)
{
  char  *k = (char*) &key;
  hash_t h = 2139062143;

  for ( int i = 0; i < sizeof (key); ++i )
    h = h * 37 + k[i];

  return (hash_t) h;
}
static hash_t  hashtable_walk (hashtable *ht, ht_rec *data, bool isget)
{
  hash_t he = hash (data->key) % ht->lines_count;
  hash_t h = he, h_ret = h;
  //-----------------------------------------
#ifdef _DEBUG_HASH
  printf ("hash=% 3d  key=% 3d  ttl=% 2lf", he, data->key, difftime (time (NULL), data->ttl));
  if ( isget ) printf ("\nh=%d he=%d [h]=%d", h, he, ht->lines[h].busy);
#endif // _DEBUG_HASH

  /* идём по открытой адресации */
  while ( (h < ht->lines_count) && ht->lines[h].busy )
  {
    hash_t hn = hash (ht->lines[h + 1U].data.key) % ht->lines_count;
    /* если встречаем оконченный ttl */
    if ( ttl_completed (ht->lines[h].data.ttl) )
    {
#ifdef _DEBUG_HASH
      if ( isget ) printf ("\nh+1=%d hn=%d he=%d [h+1]=%d", h + 1, hn, he, ht->lines[h + 1U].busy);
#endif // _DEBUG_HASH

      /* если впереди нет ничего в открытой адресации */
      if ( ((h + 1U) >= ht->lines_count) || !ht->lines[h + 1U].busy || ((h + 1U) == hn)
          /* или хэш следующего элемента больше хэша текущего */
          || ((he < hn) && !isget) )
      {
        ht->lines[h].busy = false;
        h_ret = h;
        break;
      } /* нашли необходимый номер - заканчиваем */

      hash_t hc = ++h;
      /* пока в открытой адресации: индекс следующего элемента больше его хэша */
      while ( (h < ht->lines_count) && ht->lines[h].busy && (h > hn) )
      {
#ifdef _DEBUG_HASH 
        if ( isget ) printf ("h=%d hn=%d\n", h, hn);
#endif // _DEBUG_HASH    
        hn = hash (ht->lines[++h].data.key) % ht->lines_count;
      }

#ifdef _DEBUG_HASH 
      if ( isget ) printf ("h=%d hc=%d\n", h, hc);
#endif // _DEBUG_HASH

      /* сдвигаем cледующего на данное место */
      memmove (&ht->lines[hc - 1U], &ht->lines[hc], (h - hc) * sizeof (ht_line));

      ht->lines[h - 1U].busy = false;
      h = hc;
    }
    else if ( ht->lines[h].data.key == data->key )
    {
      h_ret = h;
      break;
    }
    else if ( he < h )
    {
      /* раздвинуть записи */
      hash_t hc = h;
      while ( (h < ht->lines_count) && ht->lines[h].busy )
      {
        if ( ttl_completed (ht->lines[h].data.ttl) )
        {
          ht->lines[h].busy = false;
          break;
        }
        ++h;
      }

      if ( h >= ht->lines_count )
      {
        break;
      }

      /* раздвигаем записи */
      memmove (&ht->lines[hc + 1U], &ht->lines[hc], (h - hc) * sizeof (ht_line));
      ht->lines[hc].busy = false;

      h_ret = hc;
      break;
    }

    ++h; h_ret = h;
  } // end while
  //-----------------------------------------
  return h_ret;
}
//-----------------------------------------
bool  hashtable_get (hashtable *ht, ht_rec *data)
{
#ifdef _LOCK
  mysem_lock (ht->tb_semid, 0);
#endif // _LOCK
  //-----------------------------------------
  hash_t h = hashtable_walk (ht, data, true);
  if ( (h < ht->lines_count) && ht->lines[h].busy
      && (ht->lines[h].data.key == data->key) )
  { *data = ht->lines[h].data; }
  else { h = ht->lines_count; }
  //-----------------------------------------
#ifdef _DEBUG_HASH
  FILE *f = fopen ("output.txt", "at");
  hashtable_print_debug (ht, f);
  fclose (f);
#endif // _DEBUG_HASH
  //-----------------------------------------
#ifdef _DEBUG
  printf ("get he=%d h=%d\n", he, h);
#endif // _DEBUG

#ifdef _LOCK
  mysem_unlock (ht->tb_semid, 0);
#endif // _LOCK
  //-----------------------------------------
  return (h == ht->lines_count);
}
bool  hashtable_set (hashtable *ht, ht_rec *data)
{
#ifdef _LOCK
  mysem_lock (ht->tb_semid, 0);
#endif // _LOCK
  //-----------------------------------------
  hash_t h = hashtable_walk (ht, data, false);
  if ( (h < ht->lines_count) && (!ht->lines[h].busy
    || (ht->lines[h].data.key == data->key)) )
  { ht->lines[h].data = *data;
    ht->lines[h].busy = true;
  }
  else h = ht->lines_count;
  //-----------------------------------------
#ifdef _LOCK
  mysem_unlock (ht->tb_semid, 0);
#endif // _LOCK
  //-----------------------------------------
  return (h == ht->lines_count);
}
//-----------------------------------------
#ifdef _DEBUG_HASH
void  hashtable_print_debug (hashtable *ht, FILE *f)
{
  fprintf (f, "\n");
  for ( size_t i = 0U; i < ht->lines_count; ++i )
  {
    fprintf (f, "%3u hash=%2d  %s key=%2d ttl=% 3.2lf val=%d\n", i,
             (ht->lines[i].busy ? hash (ht->lines[i].data.key) % ht->lines_count : -1),
             (ht->lines[i].busy ? "Busy" : "Free"),
              ht->lines[i].data.key,
             !ht->lines[i].data.ttl ? 0. : difftime (time (NULL), ht->lines[i].data.ttl),
              ht->lines[i].data.val);
  }
  return;
}
#endif // _DEBUG_HASH
//------------------------------------------
