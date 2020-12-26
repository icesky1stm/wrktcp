#ifndef MAIN_H
#define MAIN_H

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <math.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <netdb.h>

#include "version.h"
#include "islog.h"
#include "net.h"
#include "output.h"
#include "tcpini.h"

#include "aprintf.h"
#include "stats.h"
#include "units.h"
#include "zmalloc.h"
#include "istime.h"

static void *thread_main(void *);
static int parse_args(config *, int, char **);
static int running_sleep(config * lcfg );

static int connect_socket(thread *, connection *);
static int reconnect_socket(thread *, connection *);
static void socket_connected(aeEventLoop *, int, void *, int);
static void socket_writeable(aeEventLoop *, int, void *, int);
static void socket_readable(aeEventLoop *, int, void *, int);
static int response_complete(void * data, char * buf, size_t n);

static int record_rate(aeEventLoop *, long long, void *);


#endif /* MAIN_H */
