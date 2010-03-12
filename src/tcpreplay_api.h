/* $Id$ */

/*
 * Copyright (c) 2009 Aaron Turner.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright owners nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TCPREPLAY_API_H_
#define _TCPREPLAY_API_H_

#include "config.h"
#include "defines.h"
#include "common/sendpacket.h"
#include "common/tcpdump.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef ENABLE_DMALLOC
#include <dmalloc.h>
#endif



#ifdef __cplusplus
extern "C" {
#endif

struct tcpreplay_s; /* forward declare */

/* in memory packet cache struct */
typedef struct packet_cache_s {
    struct pcap_pkthdr pkthdr;
    u_char *pktdata;
    struct packet_cache_s *next;
} packet_cache_t;

/* packet cache header */
typedef struct file_cache_s {
    int index;
    int cached;
    packet_cache_t *packet_cache;
} file_cache_t;

/* speed mode selector */
typedef enum {
    speed_multiplier = 1,
    speed_mbpsrate,
    speed_packetrate,
    speed_topspeed,
    speed_oneatatime
} tcpreplay_speed_mode;

/* speed mode configuration */
typedef struct {
    /* speed modifiers */
    tcpreplay_speed_mode mode;
    float speed;
    int pps_multi;
    u_int32_t (*manual_callback)(struct tcpreplay_s *, char *, COUNTER);
} tcpreplay_speed_t;

/* accurate mode selector */
typedef enum {
    accurate_gtod = 0,
#ifdef HAVE_SELECT
    accurate_select = 1,
#endif
#ifdef HAVE_RDTSC
    accurate_rdtsc = 2,
#endif
#if defined HAVE_IOPERM && defined(__i386__)
    accurate_ioport = 3,
#endif
    accurate_nanosleep = 4,
#ifdef HAVE_ABSOLUTE_TIME
    accurate_abs_time = 5
#endif
} tcpreplay_accurate;

typedef enum {
    source_filename = 1,
    source_fd = 2,
    source_cache = 3
} tcpreplay_source_type;

typedef struct {
    tcpreplay_source_type type;
    int fd;
    char *filename;
} tcpreplay_source_t;

/* run-time options */
typedef struct tcpreplay_opt_s {
    /* input/output */
    char *intf1_name;
    char *intf2_name;

    tcpreplay_speed_t speed;
    u_int32_t loop;
    int sleep_accel;

    bool use_pkthdr_len;

    /* tcpprep cache data */
    COUNTER cache_packets;
    char *cachedata;
    char *comment; /* tcpprep comment */

    /* deal with MTU/packet len issues */
    int mtu;

    /* accurate mode to use */
    tcpreplay_accurate accurate;

    /* limit # of packets to send */
    COUNTER limit_send;

    /* pcap file caching */
    bool enable_file_cache;
    file_cache_t file_cache[MAX_FILES];
    bool preload_pcap;

    /* pcap files/sources to replay */
    int source_cnt;
    tcpreplay_source_t sources[MAX_FILES];

#ifdef ENABLE_VERBOSE
    /* tcpdump verbose printing */
    bool verbose;
    char *tcpdump_args;
    tcpdump_t *tcpdump;
#endif

} tcpreplay_opt_t;


/* interface */
typedef enum {
    intf1 = 1,
    intf2
} tcpreplay_intf;

/* tcpreplay context variable */
#define TCPREPLAY_ERRSTR_LEN 1024
typedef struct tcpreplay_s {
    tcpreplay_opt_t *options;
    interface_list_t *intlist;
    sendpacket_t *intf1;
    sendpacket_t *intf2;
    char errstr[TCPREPLAY_ERRSTR_LEN];
    char warnstr[TCPREPLAY_ERRSTR_LEN];
    /* status trackers */
    int cache_bit;
    int cache_byte;
    int current_source; /* current source input being replayed */

    /* counter stats */
    tcpreplay_stats_t stats;
    tcpreplay_stats_t static_stats; /* stats returned by tcpreplay_get_stats() */

    /* abort, suspend & running flags */
    volatile bool abort;
    volatile bool suspend;
    bool running;
} tcpreplay_t;


/*
 * manual callback definition:
 * ctx              = tcpreplay context
 * interface        = name of interface current packet will be sent out 
 * current_packet   = packet number to be sent out 
 *
 * Returns number of packets to send.  0 == send all remaining packets
 * Note: Your callback method is BLOCKING the main tcpreplay loop.  If you 
 * call tcpreplay_abort() from inside of your callback, you still need to 
 * return (any value) so that the main loop is released and can abort.
 */
typedef u_int32_t(*tcpreplay_manual_callback) (tcpreplay_t *ctx, char *interface, COUNTER current_packet);


char *tcpreplay_geterr(tcpreplay_t *);
char *tcpreplay_getwarn(tcpreplay_t *);

tcpreplay_t *tcpreplay_init();
void tcpreplay_close(tcpreplay_t *);

#ifdef USE_AUTOOPTS
int tcpreplay_post_args(tcpreplay_t *);
#endif

/* all these configuration functions return 0 on success and < 0 on error. */
int tcpreplay_set_interface(tcpreplay_t *, tcpreplay_intf, char *);
int tcpreplay_set_speed_mode(tcpreplay_t *, tcpreplay_speed_mode);
int tcpreplay_set_speed_speed(tcpreplay_t *, float);
int tcpreplay_set_speed_pps_multi(tcpreplay_t *, int);
int tcpreplay_set_loop(tcpreplay_t *, u_int32_t);
int tcpreplay_set_sleep_accel(tcpreplay_t *, int);
int tcpreplay_set_use_pkthdr_len(tcpreplay_t *, bool);
int tcpreplay_set_mtu(tcpreplay_t *, int);
int tcpreplay_set_accurate(tcpreplay_t *, tcpreplay_accurate);
int tcpreplay_set_limit_send(tcpreplay_t *, COUNTER);
int tcpreplay_set_file_cache(tcpreplay_t *, bool);
int tcpreplay_set_tcpprep_cache(tcpreplay_t *, char *);
int tcpreplay_add_pcapfile(tcpreplay_t *, char *);
int tcpreplay_set_preload_pcap(tcpreplay_t *, bool);

/* information */
int tcpreplay_get_source_count(tcpreplay_t *);
int tcpreplay_get_current_source(tcpreplay_t *);

/* functions controlling execution */
int tcpreplay_replay(tcpreplay_t *, int);
const tcpreplay_stats_t *tcpreplay_get_stats(tcpreplay_t *);
int tcpreplay_abort(tcpreplay_t *);
int tcpreplay_suspend(tcpreplay_t *);
int tcpreplay_restart(tcpreplay_t *);
bool tcpreplay_is_suspended(tcpreplay_t *);
bool tcpreplay_is_running(tcpreplay_t *);

/* set callback for manual stepping */
int tcpreplay_set_manual_callback(tcpreplay_t *ctx, tcpreplay_manual_callback);

/* statistic counts */
COUNTER tcpreplay_get_pkts_sent(tcpreplay_t *ctx);
COUNTER tcpreplay_get_bytes_sent(tcpreplay_t *ctx);
COUNTER tcpreplay_get_failed(tcpreplay_t *ctx);
const struct timeval *tcpreplay_get_start_time(tcpreplay_t *ctx);
const struct timeval *tcpreplay_get_end_time(tcpreplay_t *ctx);

#ifdef ENABLE_VERBOSE
int tcpreplay_set_verbose(tcpreplay_t *, bool);
int tcpreplay_set_tcpdump_args(tcpreplay_t *, char *);
int tcpreplay_set_tcpdump(tcpreplay_t *, tcpdump_t *);
#endif

/*
 * These functions are seen by the outside world, but nobody should ever use them
 * outside of internal tcpreplay API functions
 */

#define tcpreplay_seterr(x, y, ...) __tcpreplay_seterr(x, __FUNCTION__, __LINE__, __FILE__, y, __VA_ARGS__)
void __tcpreplay_seterr(tcpreplay_t *ctx, const char *func, const int line, const char *file, const char *fmt, ...);
void tcpreplay_setwarn(tcpreplay_t *ctx, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif //_TCPREPLAY_API_H_