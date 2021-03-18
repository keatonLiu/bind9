/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * See the COPYRIGHT file distributed with this work for additional
 * information regarding copyright ownership.
 */

#pragma once

#include <unistd.h>
#include <uv.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include <isc/astack.h>
#include <isc/atomic.h>
#include <isc/buffer.h>
#include <isc/condition.h>
#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/netmgr.h>
#include <isc/queue.h>
#include <isc/quota.h>
#include <isc/random.h>
#include <isc/refcount.h>
#include <isc/region.h>
#include <isc/result.h>
#include <isc/rwlock.h>
#include <isc/sockaddr.h>
#include <isc/stats.h>
#include <isc/thread.h>
#include <isc/tls.h>
#include <isc/util.h>

#include "uv-compat.h"

#define ISC_NETMGR_TID_UNKNOWN -1

#define ISC_NETMGR_TLSBUF_SIZE 65536

#if !defined(WIN32)
/*
 * New versions of libuv support recvmmsg on unices.
 * Since recvbuf is only allocated per worker allocating a bigger one is not
 * that wasteful.
 * 20 here is UV__MMSG_MAXWIDTH taken from the current libuv source, nothing
 * will break if the original value changes.
 */
#define ISC_NETMGR_RECVBUF_SIZE (20 * 65536)
#else
#define ISC_NETMGR_RECVBUF_SIZE (65536)
#endif

#define ISC_NETMGR_SENDBUF_SIZE (sizeof(uint16_t) + UINT16_MAX)

/*%
 * Regular TCP buffer size.
 */
#define NM_REG_BUF 4096

/*%
 * Larger buffer for when the regular one isn't enough; this will
 * hold two full DNS packets with lengths.  netmgr receives 64k at
 * most in TCPDNS or TLSDNS connections, so there's no risk of overrun
 * when using a buffer this size.
 */
#define NM_BIG_BUF (65535 + 2) * 2

#if defined(SO_REUSEPORT_LB) || (defined(SO_REUSEPORT) && defined(__linux__))
#define HAVE_SO_REUSEPORT_LB 1
#endif

/*
 * Define NETMGR_TRACE to activate tracing of handles and sockets.
 * This will impair performance but enables us to quickly determine,
 * if netmgr resources haven't been cleaned up on shutdown, which ones
 * are still in use.
 */
#ifdef NETMGR_TRACE
#define TRACE_SIZE 8

void
isc__nm_dump_active(isc_nm_t *nm);

#if defined(__linux__)
#include <syscall.h>
#define gettid() (uint32_t) syscall(SYS_gettid)
#elif defined(_WIN32)
#define gettid() (uint32_t) GetCurrentThreadId()
#else
#define gettid() (uint32_t) pthread_self()
#endif

#ifdef NETMGR_TRACE_VERBOSE
#define NETMGR_TRACE_LOG(format, ...)                                \
	fprintf(stderr, "%" PRIu32 ":%d:%s:%u:%s:" format, gettid(), \
		isc_nm_tid(), file, line, func, __VA_ARGS__)
#else
#define NETMGR_TRACE_LOG(format, ...) \
	(void)file;                   \
	(void)line;                   \
	(void)func;
#endif

#define FLARG_PASS , file, line, func
#define FLARG                                              \
	, const char *file __attribute__((unused)),        \
		unsigned int line __attribute__((unused)), \
		const char *func __attribute__((unused))
#define FLARG_IEVENT(ievent)              \
	const char *file = ievent->file;  \
	unsigned int line = ievent->line; \
	const char *func = ievent->func;
#define FLARG_IEVENT_PASS(ievent) \
	ievent->file = file;      \
	ievent->line = line;      \
	ievent->func = func;
#define isc__nm_uvreq_get(req, sock) \
	isc___nm_uvreq_get(req, sock, __FILE__, __LINE__, __func__)
#define isc__nm_uvreq_put(req, sock) \
	isc___nm_uvreq_put(req, sock, __FILE__, __LINE__, __func__)
#define isc__nmsocket_init(sock, mgr, type, iface)                      \
	isc___nmsocket_init(sock, mgr, type, iface, __FILE__, __LINE__, \
			    __func__)
#define isc__nmsocket_put(sockp) \
	isc___nmsocket_put(sockp, __FILE__, __LINE__, __func__)
#define isc__nmsocket_attach(sock, target) \
	isc___nmsocket_attach(sock, target, __FILE__, __LINE__, __func__)
#define isc__nmsocket_detach(socketp) \
	isc___nmsocket_detach(socketp, __FILE__, __LINE__, __func__)
#define isc__nmsocket_close(socketp) \
	isc___nmsocket_close(socketp, __FILE__, __LINE__, __func__)
#define isc__nmhandle_get(sock, peer, local) \
	isc___nmhandle_get(sock, peer, local, __FILE__, __LINE__, __func__)
#define isc__nmsocket_prep_destroy(sock) \
	isc___nmsocket_prep_destroy(sock, __FILE__, __LINE__, __func__)
#else
#define NETMGR_TRACE_LOG(format, ...)

#define FLARG_PASS
#define FLARG
#define FLARG_IEVENT(ievent)
#define FLARG_IEVENT_PASS(ievent)
#define isc__nm_uvreq_get(req, sock) isc___nm_uvreq_get(req, sock)
#define isc__nm_uvreq_put(req, sock) isc___nm_uvreq_put(req, sock)
#define isc__nmsocket_init(sock, mgr, type, iface) \
	isc___nmsocket_init(sock, mgr, type, iface)
#define isc__nmsocket_put(sockp)	   isc___nmsocket_put(sockp)
#define isc__nmsocket_attach(sock, target) isc___nmsocket_attach(sock, target)
#define isc__nmsocket_detach(socketp)	   isc___nmsocket_detach(socketp)
#define isc__nmsocket_close(socketp)	   isc___nmsocket_close(socketp)
#define isc__nmhandle_get(sock, peer, local) \
	isc___nmhandle_get(sock, peer, local)
#define isc__nmsocket_prep_destroy(sock) isc___nmsocket_prep_destroy(sock)
#endif

/*
 * Single network event loop worker.
 */
typedef struct isc__networker {
	isc_nm_t *mgr;
	int id;		  /* thread id */
	uv_loop_t loop;	  /* libuv loop structure */
	uv_async_t async; /* async channel to send
			   * data to this networker */
	isc_mutex_t lock;
	isc_condition_t cond;
	bool paused;
	bool finished;
	isc_thread_t thread;
	isc_queue_t *ievents;	   /* incoming async events */
	isc_queue_t *ievents_prio; /* priority async events
				    * used for listening etc.
				    * can be processed while
				    * worker is paused */
	isc_refcount_t references;
	atomic_int_fast64_t pktcount;
	char *recvbuf;
	char *sendbuf;
	bool recvbuf_inuse;
} isc__networker_t;

/*
 * A general handle for a connection bound to a networker.  For UDP
 * connections we have peer address here, so both TCP and UDP can be
 * handled with a simple send-like function
 */
#define NMHANDLE_MAGIC ISC_MAGIC('N', 'M', 'H', 'D')
#define VALID_NMHANDLE(t)                      \
	(ISC_MAGIC_VALID(t, NMHANDLE_MAGIC) && \
	 atomic_load(&(t)->references) > 0)

typedef void (*isc__nm_closecb)(isc_nmhandle_t *);
typedef struct isc_nm_http_session isc_nm_http_session_t;

struct isc_nmhandle {
	int magic;
	isc_refcount_t references;

	/*
	 * The socket is not 'attached' in the traditional
	 * reference-counting sense. Instead, we keep all handles in an
	 * array in the socket object.  This way, we don't have circular
	 * dependencies and we can close all handles when we're destroying
	 * the socket.
	 */
	isc_nmsocket_t *sock;
	size_t ah_pos; /* Position in the socket's 'active handles' array */

	isc_nm_http_session_t *httpsession;

	isc_sockaddr_t peer;
	isc_sockaddr_t local;
	isc_nm_opaquecb_t doreset; /* reset extra callback, external */
	isc_nm_opaquecb_t dofree;  /* free extra callback, external */
#ifdef NETMGR_TRACE
	void *backtrace[TRACE_SIZE];
	int backtrace_size;
	LINK(isc_nmhandle_t) active_link;
#endif
	void *opaque;
	char extra[];
};

/*
 * An interface - an address we can listen on.
 */
struct isc_nmiface {
	isc_sockaddr_t addr;
};

typedef enum isc__netievent_type {
	netievent_udpconnect,
	netievent_udpsend,
	netievent_udpread,
	netievent_udpstop,
	netievent_udpcancel,
	netievent_udpclose,

	netievent_tcpconnect,
	netievent_tcpsend,
	netievent_tcpstartread,
	netievent_tcppauseread,
	netievent_tcpaccept,
	netievent_tcpstop,
	netievent_tcpcancel,
	netievent_tcpclose,

	netievent_tcpdnsaccept,
	netievent_tcpdnsconnect,
	netievent_tcpdnssend,
	netievent_tcpdnsread,
	netievent_tcpdnscancel,
	netievent_tcpdnsclose,
	netievent_tcpdnsstop,

	netievent_tlsclose,
	netievent_tlssend,
	netievent_tlsstartread,
	netievent_tlsconnect,
	netievent_tlsdobio,
	netievent_tlscancel,

	netievent_tlsdnsaccept,
	netievent_tlsdnsconnect,
	netievent_tlsdnssend,
	netievent_tlsdnsread,
	netievent_tlsdnscancel,
	netievent_tlsdnsclose,
	netievent_tlsdnsstop,
	netievent_tlsdnscycle,
	netievent_tlsdnsshutdown,

	netievent_httpstop,
	netievent_httpsend,
	netievent_httpclose,

	netievent_close,
	netievent_shutdown,
	netievent_stop,
	netievent_pause,

	netievent_connectcb,
	netievent_readcb,
	netievent_sendcb,

	netievent_prio = 0xff, /* event type values higher than this
				* will be treated as high-priority
				* events, which can be processed
				* while the netmgr is paused.
				*/
	netievent_udplisten,
	netievent_tcplisten,
	netievent_tcpdnslisten,
	netievent_tlsdnslisten,
	netievent_resume,
	netievent_detach,
} isc__netievent_type;

typedef union {
	isc_nm_recv_cb_t recv;
	isc_nm_cb_t send;
	isc_nm_cb_t connect;
	isc_nm_accept_cb_t accept;
} isc__nm_cb_t;

typedef struct isc_nm_httphandler isc_nm_httphandler_t;
struct isc_nm_httphandler {
	char *path;
	isc_nm_recv_cb_t cb;
	void *cbarg;
	size_t extrahandlesize;
	LINK(isc_nm_httphandler_t) link;
};

/*
 * Wrapper around uv_req_t with 'our' fields in it.  req->data should
 * always point to its parent.  Note that we always allocate more than
 * sizeof(struct) because we make room for different req types;
 */
#define UVREQ_MAGIC    ISC_MAGIC('N', 'M', 'U', 'R')
#define VALID_UVREQ(t) ISC_MAGIC_VALID(t, UVREQ_MAGIC)

typedef struct isc__nm_uvreq isc__nm_uvreq_t;
struct isc__nm_uvreq {
	int magic;
	isc_nmsocket_t *sock;
	isc_nmhandle_t *handle;
	char tcplen[2];	      /* The TCP DNS message length */
	uv_buf_t uvbuf;	      /* translated isc_region_t, to be
			       * sent or received */
	isc_sockaddr_t local; /* local address */
	isc_sockaddr_t peer;  /* peer address */
	isc__nm_cb_t cb;      /* callback */
	void *cbarg;	      /* callback argument */
	uv_pipe_t ipc;	      /* used for sending socket
			       * uv_handles to other threads */
	union {
		uv_handle_t handle;
		uv_req_t req;
		uv_getaddrinfo_t getaddrinfo;
		uv_getnameinfo_t getnameinfo;
		uv_shutdown_t shutdown;
		uv_write_t write;
		uv_connect_t connect;
		uv_udp_send_t udp_send;
		uv_fs_t fs;
		uv_work_t work;
	} uv_req;
	ISC_LINK(isc__nm_uvreq_t) link;
};

void *
isc__nm_get_netievent(isc_nm_t *mgr, isc__netievent_type type);
/*%<
 * Allocate an ievent and set the type.
 */
void
isc__nm_put_netievent(isc_nm_t *mgr, void *ievent);

/*
 * The macros here are used to simulate the "inheritance" in C, there's the base
 * netievent structure that contains just its own type and socket, and there are
 * extended netievent types that also have handles or requests or other data.
 *
 * The macros here ensure that:
 *
 *   1. every netievent type has matching definition, declaration and
 *      implementation
 *
 *   2. we handle all the netievent types of same subclass the same, e.g. if the
 *      extended netievent contains handle, we always attach to the handle in
 *      the ctor and detach from the handle in dtor.
 *
 * There are three macros here for each netievent subclass:
 *
 *   1. NETIEVENT_*_TYPE(type) creates the typedef for each type; used below in
 *   this header
 *
 *   2. NETIEVENT_*_DECL(type) generates the declaration of the get and put
 *      functions (isc__nm_get_netievent_* and isc__nm_put_netievent_*); used
 *      below in this header
 *
 *   3. NETIEVENT_*_DEF(type) generates the definition of the functions; used
 *   either in netmgr.c or matching protocol file (e.g. udp.c, tcp.c, etc.)
 */

#define NETIEVENT__SOCKET         \
	isc__netievent_type type; \
	isc_nmsocket_t *sock;     \
	const char *file;         \
	unsigned int line;        \
	const char *func

typedef struct isc__netievent__socket {
	NETIEVENT__SOCKET;
} isc__netievent__socket_t;

#define NETIEVENT_SOCKET_TYPE(type) \
	typedef isc__netievent__socket_t isc__netievent_##type##_t;

#define NETIEVENT_SOCKET_DECL(type)                              \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type( \
		isc_nm_t *nm, isc_nmsocket_t *sock);             \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,          \
					  isc__netievent_##type##_t *ievent);

#define NETIEVENT_SOCKET_DEF(type)                                             \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(               \
		isc_nm_t *nm, isc_nmsocket_t *sock) {                          \
		isc__netievent_##type##_t *ievent =                            \
			isc__nm_get_netievent(nm, netievent_##type);           \
		isc__nmsocket_attach(sock, &ievent->sock);                     \
                                                                               \
		return (ievent);                                               \
	}                                                                      \
                                                                               \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                        \
					  isc__netievent_##type##_t *ievent) { \
		isc__nmsocket_detach(&ievent->sock);                           \
		isc__nm_put_netievent(nm, ievent);                             \
	}

typedef struct isc__netievent__socket_req {
	NETIEVENT__SOCKET;
	isc__nm_uvreq_t *req;
} isc__netievent__socket_req_t;

#define NETIEVENT_SOCKET_REQ_TYPE(type) \
	typedef isc__netievent__socket_req_t isc__netievent_##type##_t;

#define NETIEVENT_SOCKET_REQ_DECL(type)                                    \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(           \
		isc_nm_t *nm, isc_nmsocket_t *sock, isc__nm_uvreq_t *req); \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                    \
					  isc__netievent_##type##_t *ievent);

#define NETIEVENT_SOCKET_REQ_DEF(type)                                         \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(               \
		isc_nm_t *nm, isc_nmsocket_t *sock, isc__nm_uvreq_t *req) {    \
		isc__netievent_##type##_t *ievent =                            \
			isc__nm_get_netievent(nm, netievent_##type);           \
		isc__nmsocket_attach(sock, &ievent->sock);                     \
		ievent->req = req;                                             \
                                                                               \
		return (ievent);                                               \
	}                                                                      \
                                                                               \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                        \
					  isc__netievent_##type##_t *ievent) { \
		isc__nmsocket_detach(&ievent->sock);                           \
		isc__nm_put_netievent(nm, ievent);                             \
	}

typedef struct isc__netievent__socket_req_result {
	isc__netievent_type type;
	isc_nmsocket_t *sock;
	isc__nm_uvreq_t *req;
	isc_result_t result;
} isc__netievent__socket_req_result_t;

#define NETIEVENT_SOCKET_REQ_RESULT_TYPE(type) \
	typedef isc__netievent__socket_req_result_t isc__netievent_##type##_t;

#define NETIEVENT_SOCKET_REQ_RESULT_DECL(type)                            \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(          \
		isc_nm_t *nm, isc_nmsocket_t *sock, isc__nm_uvreq_t *req, \
		isc_result_t result);                                     \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                   \
					  isc__netievent_##type##_t *ievent);

#define NETIEVENT_SOCKET_REQ_RESULT_DEF(type)                                  \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(               \
		isc_nm_t *nm, isc_nmsocket_t *sock, isc__nm_uvreq_t *req,      \
		isc_result_t result) {                                         \
		isc__netievent_##type##_t *ievent =                            \
			isc__nm_get_netievent(nm, netievent_##type);           \
		isc__nmsocket_attach(sock, &ievent->sock);                     \
		ievent->req = req;                                             \
		ievent->result = result;                                       \
                                                                               \
		return (ievent);                                               \
	}                                                                      \
                                                                               \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                        \
					  isc__netievent_##type##_t *ievent) { \
		isc__nmsocket_detach(&ievent->sock);                           \
		isc__nm_put_netievent(nm, ievent);                             \
	}

typedef struct isc__netievent__socket_handle {
	NETIEVENT__SOCKET;
	isc_nmhandle_t *handle;
} isc__netievent__socket_handle_t;

#define NETIEVENT_SOCKET_HANDLE_TYPE(type) \
	typedef isc__netievent__socket_handle_t isc__netievent_##type##_t;

#define NETIEVENT_SOCKET_HANDLE_DECL(type)                                   \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(             \
		isc_nm_t *nm, isc_nmsocket_t *sock, isc_nmhandle_t *handle); \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                      \
					  isc__netievent_##type##_t *ievent);

#define NETIEVENT_SOCKET_HANDLE_DEF(type)                                      \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(               \
		isc_nm_t *nm, isc_nmsocket_t *sock, isc_nmhandle_t *handle) {  \
		isc__netievent_##type##_t *ievent =                            \
			isc__nm_get_netievent(nm, netievent_##type);           \
		isc__nmsocket_attach(sock, &ievent->sock);                     \
		isc_nmhandle_attach(handle, &ievent->handle);                  \
                                                                               \
		return (ievent);                                               \
	}                                                                      \
                                                                               \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                        \
					  isc__netievent_##type##_t *ievent) { \
		isc__nmsocket_detach(&ievent->sock);                           \
		isc_nmhandle_detach(&ievent->handle);                          \
		isc__nm_put_netievent(nm, ievent);                             \
	}

typedef struct isc__netievent__socket_quota {
	NETIEVENT__SOCKET;
	isc_quota_t *quota;
} isc__netievent__socket_quota_t;

#define NETIEVENT_SOCKET_QUOTA_TYPE(type) \
	typedef isc__netievent__socket_quota_t isc__netievent_##type##_t;

#define NETIEVENT_SOCKET_QUOTA_DECL(type)                                \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(         \
		isc_nm_t *nm, isc_nmsocket_t *sock, isc_quota_t *quota); \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                  \
					  isc__netievent_##type##_t *ievent);

#define NETIEVENT_SOCKET_QUOTA_DEF(type)                                       \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(               \
		isc_nm_t *nm, isc_nmsocket_t *sock, isc_quota_t *quota) {      \
		isc__netievent_##type##_t *ievent =                            \
			isc__nm_get_netievent(nm, netievent_##type);           \
		isc__nmsocket_attach(sock, &ievent->sock);                     \
		ievent->quota = quota;                                         \
                                                                               \
		return (ievent);                                               \
	}                                                                      \
                                                                               \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                        \
					  isc__netievent_##type##_t *ievent) { \
		isc__nmsocket_detach(&ievent->sock);                           \
		isc__nm_put_netievent(nm, ievent);                             \
	}

typedef struct isc__netievent_udpsend {
	NETIEVENT__SOCKET;
	isc_sockaddr_t peer;
	isc__nm_uvreq_t *req;
} isc__netievent_udpsend_t;

typedef struct isc__netievent_tlsconnect {
	isc__netievent_type type;
	isc_nmsocket_t *sock;
	SSL_CTX *ctx;
	isc_sockaddr_t local; /* local address */
	isc_sockaddr_t peer;  /* peer address */
} isc__netievent_tlsconnect_t;

typedef struct isc__netievent {
	isc__netievent_type type;
} isc__netievent_t;

#define NETIEVENT_TYPE(type) typedef isc__netievent_t isc__netievent_##type##_t;

#define NETIEVENT_DECL(type)                                                   \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(isc_nm_t *nm); \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                        \
					  isc__netievent_##type##_t *ievent);

#define NETIEVENT_DEF(type)                                                    \
	isc__netievent_##type##_t *isc__nm_get_netievent_##type(               \
		isc_nm_t *nm) {                                                \
		isc__netievent_##type##_t *ievent =                            \
			isc__nm_get_netievent(nm, netievent_##type);           \
                                                                               \
		return (ievent);                                               \
	}                                                                      \
                                                                               \
	void isc__nm_put_netievent_##type(isc_nm_t *nm,                        \
					  isc__netievent_##type##_t *ievent) { \
		isc__nm_put_netievent(nm, ievent);                             \
	}

typedef union {
	isc__netievent_t ni;
	isc__netievent__socket_t nis;
	isc__netievent__socket_req_t nisr;
	isc__netievent_udpsend_t nius;
	isc__netievent__socket_quota_t nisq;
	isc__netievent_tlsconnect_t nitc;
} isc__netievent_storage_t;

/*
 * Network manager
 */
#define NM_MAGIC    ISC_MAGIC('N', 'E', 'T', 'M')
#define VALID_NM(t) ISC_MAGIC_VALID(t, NM_MAGIC)

struct isc_nm {
	int magic;
	isc_refcount_t references;
	isc_mem_t *mctx;
	uint32_t nworkers;
	isc_mutex_t lock;
	isc_condition_t wkstatecond;
	isc__networker_t *workers;

	isc_stats_t *stats;

	isc_mempool_t *reqpool;
	isc_mutex_t reqlock;

	isc_mempool_t *evpool;
	isc_mutex_t evlock;

	uint_fast32_t workers_running;
	uint_fast32_t workers_paused;
	atomic_uint_fast32_t maxudp;

	/*
	 * Active connections are being closed and new connections are
	 * no longer allowed.
	 */
	atomic_bool closing;

	/*
	 * A worker is actively waiting for other workers, for example to
	 * stop listening; that means no other thread can do the same thing
	 * or pause, or we'll deadlock. We have to either re-enqueue our
	 * event or wait for the other one to finish if we want to pause.
	 */
	atomic_bool interlocked;

	/*
	 * Timeout values for TCP connections, corresponding to
	 * tcp-intiial-timeout, tcp-idle-timeout, tcp-keepalive-timeout,
	 * and tcp-advertised-timeout. Note that these are stored in
	 * milliseconds so they can be used directly with the libuv timer,
	 * but they are configured in tenths of seconds.
	 */
	atomic_uint_fast32_t init;
	atomic_uint_fast32_t idle;
	atomic_uint_fast32_t keepalive;
	atomic_uint_fast32_t advertised;

#ifdef NETMGR_TRACE
	ISC_LIST(isc_nmsocket_t) active_sockets;
#endif
};

typedef enum isc_nmsocket_type {
	isc_nm_udpsocket,
	isc_nm_udplistener, /* Aggregate of nm_udpsocks */
	isc_nm_tcpsocket,
	isc_nm_tcplistener,
	isc_nm_tcpdnslistener,
	isc_nm_tcpdnssocket,
	isc_nm_tlslistener,
	isc_nm_tlssocket,
	isc_nm_tlsdnslistener,
	isc_nm_tlsdnssocket,
	isc_nm_httplistener,
	isc_nm_httpsocket
} isc_nmsocket_type;

/*%
 * A universal structure for either a single socket or a group of
 * dup'd/SO_REUSE_PORT-using sockets listening on the same interface.
 */
#define NMSOCK_MAGIC	ISC_MAGIC('N', 'M', 'S', 'K')
#define VALID_NMSOCK(t) ISC_MAGIC_VALID(t, NMSOCK_MAGIC)

/*%
 * Index into socket stat counter arrays.
 */
enum {
	STATID_OPEN = 0,
	STATID_OPENFAIL = 1,
	STATID_CLOSE = 2,
	STATID_BINDFAIL = 3,
	STATID_CONNECTFAIL = 4,
	STATID_CONNECT = 5,
	STATID_ACCEPTFAIL = 6,
	STATID_ACCEPT = 7,
	STATID_SENDFAIL = 8,
	STATID_RECVFAIL = 9,
	STATID_ACTIVE = 10
};

typedef struct isc_nmsocket_tls_send_req {
	isc_nmsocket_t *tlssock;
	isc_region_t data;
	isc_nm_cb_t cb;
	void *cbarg;
	isc_nmhandle_t *handle;
	bool finish;
} isc_nmsocket_tls_send_req_t;

typedef enum isc_http_request_type {
	ISC_HTTP_REQ_GET,
	ISC_HTTP_REQ_POST,
	ISC_HTTP_REQ_UNSUPPORTED
} isc_http_request_type_t;

typedef enum isc_http_scheme_type {
	ISC_HTTP_SCHEME_HTTP,
	ISC_HTTP_SCHEME_HTTP_SECURE,
	ISC_HTTP_SCHEME_UNSUPPORTED
} isc_http_scheme_type_t;

typedef struct isc_nm_httpcbarg {
	isc_nm_recv_cb_t cb;
	void *cbarg;
	LINK(struct isc_nm_httpcbarg) link;
} isc_nm_httpcbarg_t;

typedef struct isc_nmsocket_h2 {
	isc_nmsocket_t *psock; /* owner of the structure */
	char *request_path;
	char *query_data;
	size_t query_data_len;
	bool query_too_large;
	isc_nm_httphandler_t *handler;

	uint8_t *buf;
	size_t bufsize;
	size_t bufpos;

	int32_t stream_id;
	isc_nm_http_session_t *session;

	isc_nmsocket_t *httpserver;

	isc_http_request_type_t request_type;
	isc_http_scheme_type_t request_scheme;

	size_t content_length;
	char clenbuf[128];

	int headers_error_code;
	size_t headers_data_processed;

	isc_nm_recv_cb_t cb;
	void *cbarg;
	LINK(struct isc_nmsocket_h2) link;

	ISC_LIST(isc_nm_httphandler_t) handlers;
	ISC_LIST(isc_nm_httpcbarg_t) handler_cbargs;
	isc_rwlock_t lock;

	struct {
		char *uri;
		bool post;
		isc_tlsctx_t *tlsctx;
		isc_nmiface_t local_interface;
		void *cstream;
	} connect;
} isc_nmsocket_h2_t;

typedef void (*isc_nm_closehandlecb_t)(void *arg);
/*%<
 * Opaque callback function, used for isc_nmhandle 'reset' and 'free'
 * callbacks.
 */

struct isc_nmsocket {
	/*% Unlocked, RO */
	int magic;
	int tid;
	isc_nmsocket_type type;
	isc_nm_t *mgr;

	/*% Parent socket for multithreaded listeners */
	isc_nmsocket_t *parent;
	/*% Listener socket this connection was accepted on */
	isc_nmsocket_t *listener;
	/*% Self socket */
	isc_nmsocket_t *self;

	/*% TLS stuff */
	struct tls {
		isc_tls_t *tls;
		isc_tlsctx_t *ctx;
		BIO *app_rbio;
		BIO *app_wbio;
		BIO *ssl_rbio;
		BIO *ssl_wbio;
		enum {
			TLS_STATE_NONE,
			TLS_STATE_HANDSHAKE,
			TLS_STATE_IO,
			TLS_STATE_ERROR,
			TLS_STATE_CLOSING
		} state;
		uv_buf_t senddata;
		bool cycle;
		isc_result_t pending_error;
		/* List of active send requests. */
		isc__nm_uvreq_t *pending_req;
	} tls;

	/*% TLS stuff */
	struct tlsstream {
		bool server;
		BIO *app_bio;
		isc_tls_t *tls;
		isc_tlsctx_t *ctx;
		BIO *ssl_bio;
		isc_nmsocket_t *tlslistener;
		isc_nmiface_t server_iface;
		isc_nmiface_t local_iface;
		bool connect_from_networker;
		atomic_bool result_updated;
		enum {
			TLS_INIT,
			TLS_HANDSHAKE,
			TLS_IO,
			TLS_CLOSING,
			TLS_CLOSED
		} state; /*%< The order of these is significant */
		size_t nsending;
	} tlsstream;

	isc_nmsocket_h2_t h2;
	/*%
	 * quota is the TCP client, attached when a TCP connection
	 * is established. pquota is a non-attached pointer to the
	 * TCP client quota, stored in listening sockets but only
	 * attached in connected sockets.
	 */
	isc_quota_t *quota;
	isc_quota_t *pquota;
	isc_quota_cb_t quotacb;

	/*%
	 * Socket statistics
	 */
	const isc_statscounter_t *statsindex;

	/*%
	 * TCP read/connect timeout timers.
	 */
	uv_timer_t timer;
	bool timer_initialized;
	bool timer_running;
	uint64_t read_timeout;
	uint64_t connect_timeout;

	/*% outer socket is for 'wrapped' sockets - e.g. tcpdns in tcp */
	isc_nmsocket_t *outer;

	/*% server socket for connections */
	isc_nmsocket_t *server;

	/*% Child sockets for multi-socket setups */
	isc_nmsocket_t *children;
	uint_fast32_t nchildren;
	isc_nmiface_t *iface;
	isc_nmhandle_t *statichandle;
	isc_nmhandle_t *outerhandle;

	/*% Extra data allocated at the end of each isc_nmhandle_t */
	size_t extrahandlesize;

	/*% TCP backlog */
	int backlog;

	/*% libuv data */
	uv_os_sock_t fd;
	union uv_any_handle uv_handle;

	/*% Peer address */
	isc_sockaddr_t peer;

	/* Atomic */
	/*% Number of running (e.g. listening) child sockets */
	uint_fast32_t rchildren;

	/*%
	 * Socket is active if it's listening, working, etc. If it's
	 * closing, then it doesn't make a sense, for example, to
	 * push handles or reqs for reuse.
	 */
	atomic_bool active;
	atomic_bool destroying;

	/*%
	 * Socket is closed if it's not active and all the possible
	 * callbacks were fired, there are no active handles, etc.
	 * If active==false but closed==false, that means the socket
	 * is closing.
	 */
	atomic_bool closing;
	atomic_bool closed;
	atomic_bool listening;
	atomic_bool connecting;
	atomic_bool connected;
	bool accepting;
	bool reading;
	isc_refcount_t references;

	/*%
	 * Established an outgoing connection, as client not server.
	 */
	atomic_bool client;

	/*%
	 * TCPDNS socket has been set not to pipeline.
	 */
	atomic_bool sequential;

	/*%
	 * The socket is processing read callback, this is guard to not read
	 * data before the readcb is back.
	 */
	bool processing;

	/*%
	 * A TCP socket has had isc_nm_pauseread() called.
	 */
	atomic_bool readpaused;

	/*%
	 * A TCP or TCPDNS socket has been set to use the keepalive
	 * timeout instead of the default idle timeout.
	 */
	atomic_bool keepalive;

	/*%
	 * 'spare' handles for that can be reused to avoid allocations,
	 * for UDP.
	 */
	isc_astack_t *inactivehandles;
	isc_astack_t *inactivereqs;

	/*%
	 * Used to wait for TCP listening events to complete, and
	 * for the number of running children to reach zero during
	 * shutdown.
	 *
	 * We use two condition variables to prevent the race where the netmgr
	 * threads would be able to finish and destroy the socket before it's
	 * unlocked by the isc_nm_listen<proto>() function.  So, the flow is as
	 * follows:
	 *
	 *   1. parent thread creates all children sockets and passes then to
	 *      netthreads, looks at the signaling variable and WAIT(cond) until
	 *      the childrens are done initializing
	 *
	 *   2. the events get picked by netthreads, calls the libuv API (and
	 *      either succeeds or fails) and WAIT(scond) until all other
	 *      children sockets in netthreads are initialized and the listening
	 *      socket lock is unlocked
	 *
	 *   3. the control is given back to the parent thread which now either
	 *      returns success or shutdowns the listener if an error has
	 *      occured in the children netthread
	 *
	 * NOTE: The other approach would be doing an extra attach to the parent
	 * listening socket, and then detach it in the parent thread, but that
	 * breaks the promise that once the libuv socket is initialized on the
	 * nmsocket, the nmsocket needs to be handled only by matching
	 * netthread, so in fact that would add a complexity in a way that
	 * isc__nmsocket_detach would have to be converted to use an
	 * asynchrounous netievent.
	 */
	isc_mutex_t lock;
	isc_condition_t cond;
	isc_condition_t scond;

	/*%
	 * Used to pass a result back from listen or connect events.
	 */
	isc_result_t result;

	/*%
	 * List of active handles.
	 * ah - current position in 'ah_frees'; this represents the
	 *	current number of active handles;
	 * ah_size - size of the 'ah_frees' and 'ah_handles' arrays
	 * ah_handles - array pointers to active handles
	 *
	 * Adding a handle
	 *  - if ah == ah_size, reallocate
	 *  - x = ah_frees[ah]
	 *  - ah_frees[ah++] = 0;
	 *  - ah_handles[x] = handle
	 *  - x must be stored with the handle!
	 * Removing a handle:
	 *  - ah_frees[--ah] = x
	 *  - ah_handles[x] = NULL;
	 *
	 * XXX: for now this is locked with socket->lock, but we
	 * might want to change it to something lockless in the
	 * future.
	 */
	atomic_int_fast32_t ah;
	size_t ah_size;
	size_t *ah_frees;
	isc_nmhandle_t **ah_handles;

	/*% Buffer for TCPDNS processing */
	size_t buf_size;
	size_t buf_len;
	unsigned char *buf;

	/*%
	 * This function will be called with handle->sock
	 * as the argument whenever a handle's references drop
	 * to zero, after its reset callback has been called.
	 */
	isc_nm_closehandlecb_t closehandle_cb;

	isc_nmhandle_t *recv_handle;
	isc_nm_recv_cb_t recv_cb;
	void *recv_cbarg;
	bool recv_read;

	isc_nm_cb_t connect_cb;
	void *connect_cbarg;

	isc_nm_accept_cb_t accept_cb;
	void *accept_cbarg;

	atomic_int_fast32_t active_child_connections;

#ifdef NETMGR_TRACE
	void *backtrace[TRACE_SIZE];
	int backtrace_size;
	LINK(isc_nmsocket_t) active_link;
	ISC_LIST(isc_nmhandle_t) active_handles;
#endif
};

bool
isc__nm_in_netthread(void);
/*%
 * Returns 'true' if we're in the network thread.
 */

void
isc__nm_maybe_enqueue_ievent(isc__networker_t *worker, isc__netievent_t *event);
/*%<
 * If the caller is already in the matching nmthread, process the netievent
 * directly, if not enqueue using isc__nm_enqueue_ievent().
 */

void
isc__nm_enqueue_ievent(isc__networker_t *worker, isc__netievent_t *event);
/*%<
 * Enqueue an ievent onto a specific worker queue. (This the only safe
 * way to use an isc__networker_t from another thread.)
 */

void
isc__nm_free_uvbuf(isc_nmsocket_t *sock, const uv_buf_t *buf);
/*%<
 * Free a buffer allocated for a receive operation.
 *
 * Note that as currently implemented, this doesn't actually
 * free anything, marks the isc__networker's UDP receive buffer
 * as "not in use".
 */

isc_nmhandle_t *
isc___nmhandle_get(isc_nmsocket_t *sock, isc_sockaddr_t *peer,
		   isc_sockaddr_t *local FLARG);
/*%<
 * Get a handle for the socket 'sock', allocating a new one
 * if there isn't one available in 'sock->inactivehandles'.
 *
 * If 'peer' is not NULL, set the handle's peer address to 'peer',
 * otherwise set it to 'sock->peer'.
 *
 * If 'local' is not NULL, set the handle's local address to 'local',
 * otherwise set it to 'sock->iface->addr'.
 *
 * 'sock' will be attached to 'handle->sock'. The caller may need
 * to detach the socket afterward.
 */

isc__nm_uvreq_t *
isc___nm_uvreq_get(isc_nm_t *mgr, isc_nmsocket_t *sock FLARG);
/*%<
 * Get a UV request structure for the socket 'sock', allocating a
 * new one if there isn't one available in 'sock->inactivereqs'.
 */

void
isc___nm_uvreq_put(isc__nm_uvreq_t **req, isc_nmsocket_t *sock FLARG);
/*%<
 * Completes the use of a UV request structure, setting '*req' to NULL.
 *
 * The UV request is pushed onto the 'sock->inactivereqs' stack or,
 * if that doesn't work, freed.
 */

void
isc___nmsocket_init(isc_nmsocket_t *sock, isc_nm_t *mgr, isc_nmsocket_type type,
		    isc_nmiface_t *iface FLARG);
/*%<
 * Initialize socket 'sock', attach it to 'mgr', and set it to type 'type'
 * and its interface to 'iface'.
 */

void
isc___nmsocket_attach(isc_nmsocket_t *sock, isc_nmsocket_t **target FLARG);
/*%<
 * Attach to a socket, increasing refcount
 */

void
isc___nmsocket_detach(isc_nmsocket_t **socketp FLARG);
/*%<
 * Detach from socket, decreasing refcount and possibly destroying the
 * socket if it's no longer referenced.
 */

void
isc___nmsocket_prep_destroy(isc_nmsocket_t *sock FLARG);
/*%<
 * Market 'sock' as inactive, close it if necessary, and destroy it
 * if there are no remaining references or active handles.
 */

bool
isc__nmsocket_active(isc_nmsocket_t *sock);
/*%<
 * Determine whether 'sock' is active by checking 'sock->active'
 * or, for child sockets, 'sock->parent->active'.
 */

bool
isc__nmsocket_deactivate(isc_nmsocket_t *sock);
/*%<
 * @brief Deactivate active socket
 *
 * Atomically deactive the socket by setting @p sock->active or, for child
 * sockets, @p sock->parent->active to @c false
 *
 * @param[in] sock - valid nmsocket
 * @return @c false if the socket was already inactive, @c true otherwise
 */

void
isc__nmsocket_clearcb(isc_nmsocket_t *sock);
/*%<
 * Clear the recv and accept callbacks in 'sock'.
 */

void
isc__nmsocket_timer_stop(isc_nmsocket_t *sock);
void
isc__nmsocket_timer_start(isc_nmsocket_t *sock);
void
isc__nmsocket_timer_restart(isc_nmsocket_t *sock);
/*%<
 * Start/stop/restart the read timeout on the socket
 */

void
isc__nm_connectcb(isc_nmsocket_t *sock, isc__nm_uvreq_t *uvreq,
		  isc_result_t eresult);

void
isc__nm_connectcb_force_async(isc_nmsocket_t *sock, isc__nm_uvreq_t *uvreq,
			      isc_result_t eresult);

void
isc__nm_async_connectcb(isc__networker_t *worker, isc__netievent_t *ev0);
/*%<
 * Issue a connect callback on the socket, used to call the callback
 */

void
isc__nm_readcb(isc_nmsocket_t *sock, isc__nm_uvreq_t *uvreq,
	       isc_result_t eresult);
void
isc__nm_async_readcb(isc__networker_t *worker, isc__netievent_t *ev0);

/*%<
 * Issue a read callback on the socket, used to call the callback
 * on failed conditions when the event can't be scheduled on the uv loop.
 *
 */

void
isc__nm_sendcb(isc_nmsocket_t *sock, isc__nm_uvreq_t *uvreq,
	       isc_result_t eresult, bool async);
void
isc__nm_async_sendcb(isc__networker_t *worker, isc__netievent_t *ev0);
/*%<
 * Issue a write callback on the socket, used to call the callback
 * on failed conditions when the event can't be scheduled on the uv loop.
 */

void
isc__nm_async_shutdown(isc__networker_t *worker, isc__netievent_t *ev0);
/*%<
 * Walk through all uv handles, get the underlying sockets and issue
 * close on them.
 */

void
isc__nm_udp_send(isc_nmhandle_t *handle, const isc_region_t *region,
		 isc_nm_cb_t cb, void *cbarg);
/*%<
 * Back-end implementation of isc_nm_send() for UDP handles.
 */

void
isc__nm_udp_read(isc_nmhandle_t *handle, isc_nm_recv_cb_t cb, void *cbarg);
/*
 * Back-end implementation of isc_nm_read() for UDP handles.
 */

void
isc__nm_udp_close(isc_nmsocket_t *sock);
/*%<
 * Close a UDP socket.
 */

void
isc__nm_udp_cancelread(isc_nmhandle_t *handle);
/*%<
 * Stop reading on a connected UDP handle.
 */

void
isc__nm_udp_shutdown(isc_nmsocket_t *sock);
/*%<
 * Called during the shutdown process to close and clean up connected
 * sockets.
 */

void
isc__nm_udp_stoplistening(isc_nmsocket_t *sock);
/*%<
 * Stop listening on 'sock'.
 */

void
isc__nm_udp_settimeout(isc_nmhandle_t *handle, uint32_t timeout);
/*%<
 * Set or clear the recv timeout for the UDP socket associated with 'handle'.
 */

void
isc__nm_async_udplisten(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_udpconnect(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_udpstop(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_udpsend(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_udpread(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_udpcancel(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_udpclose(isc__networker_t *worker, isc__netievent_t *ev0);
/*%<
 * Callback handlers for asynchronous UDP events (listen, stoplisten, send).
 */

void
isc__nm_tcp_send(isc_nmhandle_t *handle, const isc_region_t *region,
		 isc_nm_cb_t cb, void *cbarg);
/*%<
 * Back-end implementation of isc_nm_send() for TCP handles.
 */

void
isc__nm_tcp_read(isc_nmhandle_t *handle, isc_nm_recv_cb_t cb, void *cbarg);
/*
 * Back-end implementation of isc_nm_read() for TCP handles.
 */

void
isc__nm_tcp_close(isc_nmsocket_t *sock);
/*%<
 * Close a TCP socket.
 */
void
isc__nm_tcp_pauseread(isc_nmhandle_t *handle);
/*%<
 * Pause reading on this handle, while still remembering the callback.
 */

void
isc__nm_tcp_resumeread(isc_nmhandle_t *handle);
/*%<
 * Resume reading from socket.
 *
 */

void
isc__nm_tcp_shutdown(isc_nmsocket_t *sock);
/*%<
 * Called during the shutdown process to close and clean up connected
 * sockets.
 */

void
isc__nm_tcp_cancelread(isc_nmhandle_t *handle);
/*%<
 * Stop reading on a connected TCP handle.
 */

void
isc__nm_tcp_stoplistening(isc_nmsocket_t *sock);
/*%<
 * Stop listening on 'sock'.
 */

int_fast32_t
isc__nm_tcp_listener_nactive(isc_nmsocket_t *sock);
/*%<
 * Returns the number of active connections for the TCP listener socket.
 */

void
isc__nm_tcp_settimeout(isc_nmhandle_t *handle, uint32_t timeout);
/*%<
 * Set the read timeout for the TCP socket associated with 'handle'.
 */

void
isc__nm_async_tcpconnect(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcplisten(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpaccept(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpstop(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpsend(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_startread(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_pauseread(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpstartread(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcppauseread(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpcancel(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpclose(isc__networker_t *worker, isc__netievent_t *ev0);
/*%<
 * Callback handlers for asynchronous TCP events (connect, listen,
 * stoplisten, send, read, pause, close).
 */

void
isc__nm_async_tlsclose(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_async_tlssend(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_async_tlsconnect(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_async_tlsstartread(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_async_tlsdobio(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_async_tlscancel(isc__networker_t *worker, isc__netievent_t *ev0);

/*%<
 * Callback handlers for asynchronouse TLS events.
 */

void
isc__nm_async_tcpdnsaccept(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpdnsconnect(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpdnslisten(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_tcpdns_send(isc_nmhandle_t *handle, isc_region_t *region,
		    isc_nm_cb_t cb, void *cbarg);
/*%<
 * Back-end implementation of isc_nm_send() for TCPDNS handles.
 */

void
isc__nm_tcpdns_shutdown(isc_nmsocket_t *sock);

void
isc__nm_tcpdns_close(isc_nmsocket_t *sock);
/*%<
 * Close a TCPDNS socket.
 */

void
isc__nm_tcpdns_stoplistening(isc_nmsocket_t *sock);
/*%<
 * Stop listening on 'sock'.
 */

void
isc__nm_tcpdns_settimeout(isc_nmhandle_t *handle, uint32_t timeout);
/*%<
 * Set the read timeout and reset the timer for the TCPDNS socket
 * associated with 'handle', and the TCP socket it wraps around.
 */

void
isc__nm_async_tcpdnslisten(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpdnsaccept(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpdnscancel(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpdnsclose(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpdnssend(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tcpdnsstop(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_async_tcpdnsread(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_tcpdns_read(isc_nmhandle_t *handle, isc_nm_recv_cb_t cb, void *cbarg);

void
isc__nm_tcpdns_cancelread(isc_nmhandle_t *handle);
/*%<
 * Stop reading on a connected TCPDNS handle.
 */

void
isc__nm_async_tlsdnscycle(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tlsdnsaccept(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tlsdnsconnect(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tlsdnslisten(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_tlsdns_send(isc_nmhandle_t *handle, isc_region_t *region,
		    isc_nm_cb_t cb, void *cbarg);

void
isc__nm_tls_send(isc_nmhandle_t *handle, const isc_region_t *region,
		 isc_nm_cb_t cb, void *cbarg);

void
isc__nm_tls_cancelread(isc_nmhandle_t *handle);

/*%<
 * Back-end implementation of isc_nm_send() for TLSDNS handles.
 */

void
isc__nm_tlsdns_shutdown(isc_nmsocket_t *sock);

void
isc__nm_tlsdns_close(isc_nmsocket_t *sock);
/*%<
 * Close a TLSDNS socket.
 */

void
isc__nm_tlsdns_stoplistening(isc_nmsocket_t *sock);
/*%<
 * Stop listening on 'sock'.
 */

void
isc__nm_tlsdns_settimeout(isc_nmhandle_t *handle, uint32_t timeout);
/*%<
 * Set the read timeout and reset the timer for the TLSDNS socket
 * associated with 'handle', and the TCP socket it wraps around.
 */

void
isc__nm_async_tlsdnslisten(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tlsdnsaccept(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tlsdnscancel(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tlsdnsclose(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tlsdnssend(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tlsdnsstop(isc__networker_t *worker, isc__netievent_t *ev0);
void
isc__nm_async_tlsdnsshutdown(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_async_tlsdnsread(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_tlsdns_read(isc_nmhandle_t *handle, isc_nm_recv_cb_t cb, void *cbarg);

void
isc__nm_tlsdns_cancelread(isc_nmhandle_t *handle);
/*%<
 * Stop reading on a connected TLSDNS handle.
 */

void
isc__nm_tls_read(isc_nmhandle_t *handle, isc_nm_recv_cb_t cb, void *cbarg);

void
isc__nm_tls_close(isc_nmsocket_t *sock);
/*%<
 * Close a TLS socket.
 */

void
isc__nm_tls_pauseread(isc_nmhandle_t *handle);
/*%<
 * Pause reading on this handle, while still remembering the callback.
 */

void
isc__nm_tls_resumeread(isc_nmhandle_t *handle);
/*%<
 * Resume reading from the handle.
 *
 */

void
isc__nm_tls_cleanup_data(isc_nmsocket_t *sock);

void
isc__nm_tls_stoplistening(isc_nmsocket_t *sock);

void
isc__nm_tls_settimeout(isc_nmhandle_t *handle, uint32_t timeout);
void
isc__nm_tls_cleartimeout(isc_nmhandle_t *handle);
/*%<
 * Set the read timeout and reset the timer for the socket
 * associated with 'handle', and the TCP socket it wraps
 * around.
 */

void
isc__nm_http_stoplistening(isc_nmsocket_t *sock);

void
isc__nm_http_settimeout(isc_nmhandle_t *handle, uint32_t timeout);
void
isc__nm_http_cleartimeout(isc_nmhandle_t *handle);
/*%<
 * Set the read timeout and reset the timer for the socket
 * associated with 'handle', and the TLS/TCP socket it wraps
 * around.
 */

void
isc__nm_http_initsocket(isc_nmsocket_t *sock);

void
isc__nm_http_cleanup_data(isc_nmsocket_t *sock);

isc_result_t
isc__nm_http_request(isc_nmhandle_t *handle, isc_region_t *region,
		     isc_nm_recv_cb_t reply_cb, void *cbarg);

void
isc__nm_http_send(isc_nmhandle_t *handle, const isc_region_t *region,
		  isc_nm_cb_t cb, void *cbarg);

void
isc__nm_http_read(isc_nmhandle_t *handle, isc_nm_recv_cb_t cb, void *cbarg);

void
isc__nm_http_close(isc_nmsocket_t *sock);

void
isc__nm_async_httpsend(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_async_httpstop(isc__networker_t *worker, isc__netievent_t *ev0);

void
isc__nm_async_httpclose(isc__networker_t *worker, isc__netievent_t *ev0);

bool
isc__nm_parse_httpquery(const char *query_string, const char **start,
			size_t *len);

char *
isc__nm_base64url_to_base64(isc_mem_t *mem, const char *base64url,
			    const size_t base64url_len, size_t *res_len);

char *
isc__nm_base64_to_base64url(isc_mem_t *mem, const char *base64,
			    const size_t base64_len, size_t *res_len);

void
isc__nm_httpsession_attach(isc_nm_http_session_t *source,
			   isc_nm_http_session_t **targetp);
void
isc__nm_httpsession_detach(isc_nm_http_session_t **sessionp);

#define isc__nm_uverr2result(x) \
	isc___nm_uverr2result(x, true, __FILE__, __LINE__, __func__)
isc_result_t
isc___nm_uverr2result(int uverr, bool dolog, const char *file,
		      unsigned int line, const char *func);
/*%<
 * Convert a libuv error value into an isc_result_t.  The
 * list of supported error values is not complete; new users
 * of this function should add any expected errors that are
 * not already there.
 */

bool
isc__nm_acquire_interlocked(isc_nm_t *mgr);
/*%<
 * Try to acquire interlocked state; return true if successful.
 */

void
isc__nm_drop_interlocked(isc_nm_t *mgr);
/*%<
 * Drop interlocked state; signal waiters.
 */

void
isc__nm_acquire_interlocked_force(isc_nm_t *mgr);
/*%<
 * Actively wait for interlocked state.
 */

void
isc__nm_incstats(isc_nm_t *mgr, isc_statscounter_t counterid);
/*%<
 * Increment socket-related statistics counters.
 */

void
isc__nm_decstats(isc_nm_t *mgr, isc_statscounter_t counterid);
/*%<
 * Decrement socket-related statistics counters.
 */

isc_result_t
isc__nm_socket(int domain, int type, int protocol, uv_os_sock_t *sockp);
/*%<
 * Platform independent socket() version
 */

void
isc__nm_closesocket(uv_os_sock_t sock);
/*%<
 * Platform independent closesocket() version
 */

isc_result_t
isc__nm_socket_freebind(uv_os_sock_t fd, sa_family_t sa_family);
/*%<
 * Set the IP_FREEBIND (or equivalent) socket option on the uv_handle
 */

isc_result_t
isc__nm_socket_reuse(uv_os_sock_t fd);
/*%<
 * Set the SO_REUSEADDR or SO_REUSEPORT (or equivalent) socket option on the fd
 */

isc_result_t
isc__nm_socket_reuse_lb(uv_os_sock_t fd);
/*%<
 * Set the SO_REUSEPORT_LB (or equivalent) socket option on the fd
 */

isc_result_t
isc__nm_socket_incoming_cpu(uv_os_sock_t fd);
/*%<
 * Set the SO_INCOMING_CPU socket option on the fd if available
 */

isc_result_t
isc__nm_socket_dontfrag(uv_os_sock_t fd, sa_family_t sa_family);
/*%<
 * Set the SO_IP_DONTFRAG (or equivalent) socket option of the fd if available
 */

isc_result_t
isc__nm_socket_connectiontimeout(uv_os_sock_t fd, int timeout_ms);
/*%<
 * Set the connection timeout in miliseconds, on non-Linux platforms,
 * the minimum value must be at least 1000 (1 second).
 */

isc_result_t
isc__nm_socket_tcp_nodelay(uv_os_sock_t fd);
/*%<
 * Disables Nagle's algorithm on a TCP socket (sets TCP_NODELAY).
 */

/*
 * typedef all the netievent types
 */

NETIEVENT_SOCKET_TYPE(close);
NETIEVENT_SOCKET_TYPE(tcpclose);
NETIEVENT_SOCKET_TYPE(tcplisten);
NETIEVENT_SOCKET_TYPE(tcppauseread);
NETIEVENT_SOCKET_TYPE(tcpstop);
NETIEVENT_SOCKET_TYPE(tlsclose);
/* NETIEVENT_SOCKET_TYPE(tlsconnect); */ /* unique type, defined independently
					  */
NETIEVENT_SOCKET_TYPE(tlsdobio);
NETIEVENT_SOCKET_TYPE(tlsstartread);
NETIEVENT_SOCKET_HANDLE_TYPE(tlscancel);
NETIEVENT_SOCKET_TYPE(udpclose);
NETIEVENT_SOCKET_TYPE(udplisten);
NETIEVENT_SOCKET_TYPE(udpread);
/* NETIEVENT_SOCKET_TYPE(udpsend); */ /* unique type, defined independently */
NETIEVENT_SOCKET_TYPE(udpstop);

NETIEVENT_SOCKET_TYPE(tcpdnsclose);
NETIEVENT_SOCKET_TYPE(tcpdnsread);
NETIEVENT_SOCKET_TYPE(tcpdnsstop);
NETIEVENT_SOCKET_TYPE(tcpdnslisten);
NETIEVENT_SOCKET_REQ_TYPE(tcpdnsconnect);
NETIEVENT_SOCKET_REQ_TYPE(tcpdnssend);
NETIEVENT_SOCKET_HANDLE_TYPE(tcpdnscancel);
NETIEVENT_SOCKET_QUOTA_TYPE(tcpdnsaccept);

NETIEVENT_SOCKET_TYPE(tlsdnsclose);
NETIEVENT_SOCKET_TYPE(tlsdnsread);
NETIEVENT_SOCKET_TYPE(tlsdnsstop);
NETIEVENT_SOCKET_TYPE(tlsdnsshutdown);
NETIEVENT_SOCKET_TYPE(tlsdnslisten);
NETIEVENT_SOCKET_REQ_TYPE(tlsdnsconnect);
NETIEVENT_SOCKET_REQ_TYPE(tlsdnssend);
NETIEVENT_SOCKET_HANDLE_TYPE(tlsdnscancel);
NETIEVENT_SOCKET_QUOTA_TYPE(tlsdnsaccept);
NETIEVENT_SOCKET_TYPE(tlsdnscycle);

NETIEVENT_SOCKET_TYPE(httpstop);
NETIEVENT_SOCKET_REQ_TYPE(httpsend);
NETIEVENT_SOCKET_TYPE(httpclose);

NETIEVENT_SOCKET_REQ_TYPE(tcpconnect);
NETIEVENT_SOCKET_REQ_TYPE(tcpsend);
NETIEVENT_SOCKET_TYPE(tcpstartread);
NETIEVENT_SOCKET_REQ_TYPE(tlssend);
NETIEVENT_SOCKET_REQ_TYPE(udpconnect);

NETIEVENT_SOCKET_REQ_RESULT_TYPE(connectcb);
NETIEVENT_SOCKET_REQ_RESULT_TYPE(readcb);
NETIEVENT_SOCKET_REQ_RESULT_TYPE(sendcb);

NETIEVENT_SOCKET_HANDLE_TYPE(detach);
NETIEVENT_SOCKET_HANDLE_TYPE(tcpcancel);
NETIEVENT_SOCKET_HANDLE_TYPE(udpcancel);

NETIEVENT_SOCKET_QUOTA_TYPE(tcpaccept);

NETIEVENT_TYPE(pause);
NETIEVENT_TYPE(resume);
NETIEVENT_TYPE(shutdown);
NETIEVENT_TYPE(stop);

/* Now declared the helper functions */

NETIEVENT_SOCKET_DECL(close);
NETIEVENT_SOCKET_DECL(tcpclose);
NETIEVENT_SOCKET_DECL(tcplisten);
NETIEVENT_SOCKET_DECL(tcppauseread);
NETIEVENT_SOCKET_DECL(tcpstartread);
NETIEVENT_SOCKET_DECL(tcpstop);
NETIEVENT_SOCKET_DECL(tlsclose);
NETIEVENT_SOCKET_DECL(tlsconnect);
NETIEVENT_SOCKET_DECL(tlsdobio);
NETIEVENT_SOCKET_DECL(tlsstartread);
NETIEVENT_SOCKET_HANDLE_DECL(tlscancel);
NETIEVENT_SOCKET_DECL(udpclose);
NETIEVENT_SOCKET_DECL(udplisten);
NETIEVENT_SOCKET_DECL(udpread);
NETIEVENT_SOCKET_DECL(udpsend);
NETIEVENT_SOCKET_DECL(udpstop);

NETIEVENT_SOCKET_DECL(tcpdnsclose);
NETIEVENT_SOCKET_DECL(tcpdnsread);
NETIEVENT_SOCKET_DECL(tcpdnsstop);
NETIEVENT_SOCKET_DECL(tcpdnslisten);
NETIEVENT_SOCKET_REQ_DECL(tcpdnsconnect);
NETIEVENT_SOCKET_REQ_DECL(tcpdnssend);
NETIEVENT_SOCKET_HANDLE_DECL(tcpdnscancel);
NETIEVENT_SOCKET_QUOTA_DECL(tcpdnsaccept);

NETIEVENT_SOCKET_DECL(tlsdnsclose);
NETIEVENT_SOCKET_DECL(tlsdnsread);
NETIEVENT_SOCKET_DECL(tlsdnsstop);
NETIEVENT_SOCKET_DECL(tlsdnsshutdown);
NETIEVENT_SOCKET_DECL(tlsdnslisten);
NETIEVENT_SOCKET_REQ_DECL(tlsdnsconnect);
NETIEVENT_SOCKET_REQ_DECL(tlsdnssend);
NETIEVENT_SOCKET_HANDLE_DECL(tlsdnscancel);
NETIEVENT_SOCKET_QUOTA_DECL(tlsdnsaccept);
NETIEVENT_SOCKET_DECL(tlsdnscycle);

NETIEVENT_SOCKET_DECL(httpstop);
NETIEVENT_SOCKET_REQ_DECL(httpsend);
NETIEVENT_SOCKET_DECL(httpclose);

NETIEVENT_SOCKET_REQ_DECL(tcpconnect);
NETIEVENT_SOCKET_REQ_DECL(tcpsend);
NETIEVENT_SOCKET_REQ_DECL(tlssend);
NETIEVENT_SOCKET_REQ_DECL(udpconnect);

NETIEVENT_SOCKET_REQ_RESULT_DECL(connectcb);
NETIEVENT_SOCKET_REQ_RESULT_DECL(readcb);
NETIEVENT_SOCKET_REQ_RESULT_DECL(sendcb);

NETIEVENT_SOCKET_HANDLE_DECL(udpcancel);
NETIEVENT_SOCKET_HANDLE_DECL(tcpcancel);
NETIEVENT_SOCKET_DECL(detach);

NETIEVENT_SOCKET_QUOTA_DECL(tcpaccept);

NETIEVENT_DECL(pause);
NETIEVENT_DECL(resume);
NETIEVENT_DECL(shutdown);
NETIEVENT_DECL(stop);

void
isc__nm_udp_failed_read_cb(isc_nmsocket_t *sock, isc_result_t result);
void
isc__nm_tcp_failed_read_cb(isc_nmsocket_t *sock, isc_result_t result);
void
isc__nm_tcpdns_failed_read_cb(isc_nmsocket_t *sock, isc_result_t result);
void
isc__nm_tlsdns_failed_read_cb(isc_nmsocket_t *sock, isc_result_t result);

isc_result_t
isc__nm_tcpdns_processbuffer(isc_nmsocket_t *sock);
isc_result_t
isc__nm_tlsdns_processbuffer(isc_nmsocket_t *sock);

isc__nm_uvreq_t *
isc__nm_get_read_req(isc_nmsocket_t *sock, isc_sockaddr_t *sockaddr);

void
isc__nm_alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf);

void
isc__nm_udp_read_cb(uv_udp_t *handle, ssize_t nrecv, const uv_buf_t *buf,
		    const struct sockaddr *addr, unsigned flags);
void
isc__nm_tcpdns_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
void
isc__nm_tlsdns_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);

void
isc__nm_start_reading(isc_nmsocket_t *sock);
void
isc__nm_stop_reading(isc_nmsocket_t *sock);
void
isc__nm_process_sock_buffer(isc_nmsocket_t *sock);
void
isc__nm_resume_processing(void *arg);
bool
isc__nm_inactive(isc_nmsocket_t *sock);

void
isc__nm_alloc_dnsbuf(isc_nmsocket_t *sock, size_t len);

void
isc__nm_failed_send_cb(isc_nmsocket_t *sock, isc__nm_uvreq_t *req,
		       isc_result_t eresult);
void
isc__nm_failed_accept_cb(isc_nmsocket_t *sock, isc_result_t eresult);
void
isc__nm_failed_connect_cb(isc_nmsocket_t *sock, isc__nm_uvreq_t *req,
			  isc_result_t eresult);
void
isc__nm_failed_read_cb(isc_nmsocket_t *sock, isc_result_t result);

#define STREAM_CLIENTS_PER_CONN 23
