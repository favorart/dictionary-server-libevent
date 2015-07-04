#include "header.h"
#include "srv.h"
#include "srv_cache.h"
//-----------------------------------------
const char*  strmyerror ()
{
  const char* strerr;
  switch ( my_errno )
  {
    default:            strerr = NULL;                         break;
    case SRV_ERR_PARAM: strerr = " : Invalid function argument."; break;
    case SRV_ERR_INPUT: strerr = " : Input incorrect user data."; break;
    case SRV_ERR_LIBEV: strerr = " : Incorrect libevent entity."; break;
    case SRV_ERR_RCMMN: strerr = " : Incorrect request command."; break;
    case SRV_ERR_RFREE: strerr = " : Dispatched a free request."; break;
    case SRV_ERR_FDTRS: strerr = " : Failure in transmiting fd."; break;
  }
  my_errno = SRV_ERR_NONE;
  return strerr;
}
//-----------------------------------------
int   set_nonblock (evutil_socket_t fd)
{
  int flags;
#ifdef O_NONBLOCK
  if ( -1 == (flags = fcntl (fd, F_GETFL, 0)) )
    flags = 0;
  return fcntl (fd, F_SETFL, flags | O_NONBLOCK);
#else
  flags = 1;
  return ioctl (fd, FIOBIO, &flags);
#endif
}
//-----------------------------------------
int   server_config_init  (srv_conf *conf, char *port, char *ip, char *work)
{
  bool fail = false;
  //-----------------------
  const uint16_t  max_port = 65535U;
  if ( !port || (conf->port = atoi (port)) > max_port )
  {
    my_errno = SRV_ERR_INPUT;
    fprintf (stderr, "%s\n", strmyerror ());
    conf->port = 8080;
  }
  //-----------------------
  if ( ip && strlen (ip) < SRV_IPSTRLENGTH )
  { strcpy (conf->ip, ip); }
  else
  {
    my_errno = SRV_ERR_INPUT;
    fprintf (stderr, "%s\n", strmyerror ());
    strcpy (conf->ip, "0.0.0.0");
  }
  //-----------------------
  conf->ptr_server_name = SRV_SERVER_NAME;
  //-----------------------
  int  bytes = 0U;
  if ( 0 >= (bytes = readlink ("/proc/self/exe", conf->server_path, FILENAME_MAX - 1U)) )
  {
    fprintf (stderr, "%s\n", strerror (errno));
    strcpy (conf->server_path, "");
  }
  else conf->server_path[bytes] = '\0';
  //-----------------------
  if ( work )
  { conf->workers_count = atoi (work); }
  else
  {
    my_errno = SRV_ERR_INPUT;
    fprintf (stderr, "%s\n", strmyerror ());
    conf->workers_count = 4U;
  }
  //-----------------------
  if ( wque_init (&conf->workqueue, conf->workers_count) )
  { perror ("work queue init");
    exit (EXIT_FAILURE);
  }

  if ( !(conf->ht = malloc (sizeof (*conf->ht))) )
  { perror ("ht malloc");
    exit (EXIT_FAILURE);
  }

  hashtable_init (conf->ht, DICT_CACHELNS, 0, my_key_comp);
  //-----------------------
CONF_FREE:;
  if ( fail )
    server_config_free (conf);
  //-----------------------
  return fail;
}
void  server_config_print (srv_conf *conf, FILE *stream)
{
  fprintf (stream, ">>> http server config:\n\n\tname: '%s'   server path: '%s'\n"
           "\tport: %u   ip: '%s'\n\n", conf->ptr_server_name,
           conf->server_path, conf->port, conf->ip);
}
void  server_config_free  (srv_conf *conf)
{
  //-----------------------
  hashtable_free (conf->ht);
  free (conf->ht);
  //-----------------------
  wque_free (&conf->workqueue);
  //-----------------------
  memset (conf, 0, sizeof (*conf));
}
//-----------------------------------------
#include <getopt.h>   /* for getopt_long finction */
int   parse_console_parameters (int argc, char **argv, srv_conf *conf)
{
  char  *conf_opt = NULL;
  static struct option long_options[] =
  {
    { "conf", required_argument, 0, 'c' },
    { 0, 0, 0, 0 }
  };
  //-----------------------
  int c, option_index = 0;
  while ( -1 != (c = getopt_long (argc, argv, "c:",
    long_options, &option_index)) )
  {
    switch ( c )
    {
      case 0:
        printf ("option %s", long_options[option_index].name);
        break;

      case 'c':
        conf_opt = optarg;
        break;

      case '?':
        /* getopt_long already printed an error message. */
        printf ("using:\n./wwwd -c|--conf <conf>\n\n");
        break;

      default:
        printf ("?? getopt returned character code 0%o ??\n", c);
        break;
    }
  }
  //-----------------------
  if ( optind < argc )
  {
    printf ("non-option ARGV-elements: ");
    while ( optind < argc )
      printf ("%s ", argv[optind++]);
    printf ("\n");
  }
  //-----------------------
  return  server_config_init (conf, NULL, NULL, NULL);
}
//-----------------------------------------
