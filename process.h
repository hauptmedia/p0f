/*
   p0f - packet capture and overall host / flow bookkeeping
   --------------------------------------------------------

   Copyright (C) 2012 by Michal Zalewski <lcamtuf@coredump.cx>

   Distributed under the terms and conditions of GNU LGPL.

 */

#ifndef _HAVE_PROCESS_H
#define _HAVE_PROCESS_H

#include <pcap.h>

#include "types.h"
#include "fp_tcp.h"
#include "fp_http.h"

/* Parsed information handed over by the pcap callback: */

struct packet_data {

  u8  ip_ver;                           /* IP_VER4, IP_VER6                   */
  u8  tcp_type;                         /* TCP_SYN, ACK, FIN, RST             */

  u8  src[16];                          /* Source address (left-aligned)      */
  u8  dst[16];                          /* Destination address (left-aligned  */

  u16 sport;                            /* Source port                        */
  u16 dport;                            /* Destination port                   */

  u8  ttl;                              /* Observed TTL                       */
  u8  tos;                              /* IP ToS value                       */

  u16 mss;                              /* Maximum segment size               */
  u16 win;                              /* Window size                        */
  u8  wscale;                           /* Window scaling                     */
  u16 tot_hdr;                          /* Total headers (for MTU calc)       */

  u8  opt_layout[MAX_TCP_OPT];          /* Ordering of TCP options            */
  u8  opt_cnt;                          /* Count of TCP options               */
  u8  opt_eol_pad;                      /* Amount of padding past EOL         */

  u32 ts1;                              /* Own timestamp                      */

  u32 quirks;                           /* QUIRK_*                            */

  u8  ip_opt_len;                       /* Length of IP options               */

  u8* payload;                          /* TCP payload                        */
  u16 pay_len;                          /* Length of TCP payload              */

  u32 seq;                              /* seq value seen                     */

};

/* IP-level quirks: */

#define QUIRK_ECN            0x00000001 /* ECN supported                      */
#define QUIRK_DF             0x00000002 /* DF used (probably PMTUD)           */
#define QUIRK_NZ_ID          0x00000004 /* Non-zero IDs when DF set           */
#define QUIRK_ZERO_ID        0x00000008 /* Zero IDs when DF not set           */
#define QUIRK_NZ_MBZ         0x00000010 /* IP "must be zero" field isn't      */
#define QUIRK_FLOW           0x00000020 /* IPv6 flows used                    */

/* Core TCP quirks: */

#define QUIRK_ZERO_SEQ       0x00001000 /* SEQ is zero                        */
#define QUIRK_NZ_ACK         0x00002000 /* ACK non-zero when ACK flag not set */
#define QUIRK_ZERO_ACK       0x00004000 /* ACK is zero when ACK flag set      */
#define QUIRK_NZ_URG         0x00008000 /* URG non-zero when URG flag not set */
#define QUIRK_URG            0x00010000 /* URG flag set                       */
#define QUIRK_PUSH           0x00020000 /* PUSH flag on a control packet      */

/* TCP option quirks: */

#define QUIRK_OPT_ZERO_TS1   0x01000000 /* Own timestamp set to zero          */
#define QUIRK_OPT_NZ_TS2     0x02000000 /* Peer timestamp non-zero on SYN     */
#define QUIRK_OPT_EOL_NZ     0x04000000 /* Non-zero padding past EOL          */
#define QUIRK_OPT_EXWS       0x08000000 /* Excessive window scaling           */
#define QUIRK_OPT_BAD        0x10000000 /* Problem parsing TCP options        */

#define SIGNATURE_LENGTH    500

/* Host record with persistent fingerprinting data: */

struct host_data {

  struct host_data *prev, *next;        /* Linked lists                       */
  struct host_data *older, *newer;
  u32 use_cnt;                          /* Number of packet_flows attached    */

  u32 first_seen;                       /* Record created (unix time)         */
  u32 last_seen;                        /* Host last seen (unix time)         */
  u32 total_conn;                       /* Total number of connections ever   */

  u8 ip_ver;                            /* Address type                       */
  u8 addr[16];                          /* Host address data                  */

  struct tcp_sig* last_syn;             /* Sig of the most recent SYN         */
  struct tcp_sig* last_synack;          /* Sig of the most recent SYN+ACK     */

  u16 mtu;                              /* MTU */

  u16 last_port;                        /* Source port on last SYN            */

  u8  distance;                         /* Last measured distance             */

  s32 last_up_min;                      /* Last computed uptime (-1 = none)   */
  u32 up_mod_days;                      /* Uptime modulo (days)               */

  /* HTTP business: */

  u16 http_req_port;                    /* Port on which response seen        */
  u16 http_resp_port;                   /* Port on which response seen        */

  u8 tcp_signature[SIGNATURE_LENGTH + 1];
  u8 http_signature[SIGNATURE_LENGTH + 1];
  u8 ssl_signature[SIGNATURE_LENGTH + 1];

  u32 ssl_remote_time;                  /* Last client timestamp from SSL     */
  s32 ssl_remote_time_drift;            /* Time drift derived from SSL        */
};

/* TCP flow record, maintained until all fingerprinting modules are happy: */

struct packet_flow {

  struct packet_flow *prev, *next;      /* Linked lists                       */
  struct packet_flow *older, *newer;
  u32 bucket;                           /* Bucket this flow belongs to        */

  struct host_data* client;             /* Requesting client                  */
  struct host_data* server;             /* Target server                      */

  u16 cli_port;                         /* Client port                        */
  u16 srv_port;                         /* Server port                        */

  u8  orig_cli_addr[16];                /* Originating Client Address         */
  u16 orig_cli_port;                    /* Originating Client port            */

  u8  acked;                            /* SYN+ACK received?                  */
  u8  sendsyn;                          /* Created by p0f-sendsyn?            */

  s16 srv_tps;                          /* Computed TS divisor (-1 = bad)     */ 
  s16 cli_tps;

  u8* request;                          /* Client-originating data            */
  u32 req_len;                          /* Captured data length               */
  u32 next_cli_seq;                     /* Next seq on cli -> srv packet      */

  u8* response;                         /* Server-originating data            */
  u32 resp_len;                         /* Captured data length               */
  u32 next_srv_seq;                     /* Next seq on srv -> cli packet      */
  u16 syn_mss;                          /* MSS on SYN packet                  */

  u32 created;                          /* Flow creation date (unix time)     */

  /* Application-level fingerprinting: */

  s8  in_http;                          /* 0 = tbd, 1 = yes, -1 = no          */

  u8  http_req_done;                    /* Done collecting req headers?       */
  u32 http_pos;                         /* Current parsing offset             */
  u8  http_gotresp1;                    /* Got initial line of a response?    */

  struct http_sig http_tmp;             /* Temporary signature                */

  s8  in_ssl;                           /* 0 = tbd, 1 = yes, -1 = no          */

};

extern u64 packet_cnt;

void parse_packet(void* junk, const struct pcap_pkthdr* hdr, const u8* data);

u8* addr_to_str(u8* data, u8 ip_ver);

u64 get_unix_time_ms(void);
u32 get_unix_time(void);

struct host_data* lookup_host(u8* addr, u8 ip_ver);

void destroy_all_hosts(void);

#endif /* !_HAVE_PROCESS_H */
