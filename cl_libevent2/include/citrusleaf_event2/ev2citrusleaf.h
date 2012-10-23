/*
 * The Aerospike libevent C interface. 
 *
 * A porting of the standard C interface into libevent land.
 *
 * This is the external, public header file
 *
 * All rights reserved
 * Brian Bulkowski, 2009
 * CitrusLeaf
 */

 
// do this both the new skool and old skool way which gives the highest correctness,
// speed, and compatibility
#pragma once
#ifndef __EV2CITRUSLEAF_H__
#define __EV2CITRUSLEAF_H__

#include <inttypes.h>
#include <stdbool.h>
#include <event2/event.h>
#include <event2/dns.h>

#include "citrusleaf/cf_digest.h"
#include "citrusleaf/cf_hooks.h"
#include "citrusleaf/cf_log.h"

#ifdef __cplusplus
extern "C" {
#endif


#define EV2CITRUSLEAF_OK	0
#define EV2CITRUSLEAF_FAIL_CLIENT_ERROR -1
#define EV2CITRUSLEAF_FAIL_TIMEOUT -2
#define EV2CITRUSLEAF_FAIL_UNKNOWN 1
#define EV2CITRUSLEAF_FAIL_NOTFOUND 2
#define EV2CITRUSLEAF_FAIL_GENERATION 3
#define EV2CITRUSLEAF_FAIL_PARAMETER 4
#define EV2CITRUSLEAF_FAIL_KEY_EXISTS 5 // if 'WRITE_ADD', could fail because already exists
#define EV2CITRUSLEAF_FAIL_BIN_EXISTS 6
#define EV2CITRUSLEAF_FAIL_CLUSTER_KEY_MISMATCH 7
#define EV2CITRUSLEAF_FAIL_PARTITION_OUT_OF_SPACE 8
#define EV2CITRUSLEAF_FAIL_SERVERSIDE_TIMEOUT 9 // internal - this is mapped to EV2CITRUSLEAF_FAIL_TIMEOUT
#define EV2CITRUSLEAF_FAIL_NOXDS 10
#define EV2CITRUSLEAF_FAIL_UNAVAILABLE 11
#define EV2CITRUSLEAF_FAIL_INCOMPATIBLE_TYPE 12 // specified operation cannot be performed on that data type
#define EV2CITRUSLEAF_FAIL_RECORD_TOO_BIG 13
#define EV2CITRUSLEAF_FAIL_KEY_BUSY 14



#define EV2CITRUSLEAF_NO_GENERATION 0xFFFFFFFF

enum ev2citrusleaf_type { CL_NULL = 0x00, CL_INT = 0x01, CL_FLOAT = 2, CL_STR = 0x03, CL_BLOB = 0x04,
	CL_TIMESTAMP = 5, CL_DIGEST = 6, CL_JAVA_BLOB = 7, CL_CSHARP_BLOB = 8, CL_PYTHON_BLOB = 9, 
	CL_RUBY_BLOB = 10, CL_UNKNOWN = 666666};
typedef enum ev2citrusleaf_type ev2citrusleaf_type;

enum ev2citrusleaf_write_policy { CL_WRITE_ASYNC, CL_WRITE_ONESHOT, CL_WRITE_RETRY, CL_WRITE_ASSURED };

typedef enum ev2citrusleaf_write_policy ev2citrusleaf_write_policy;

typedef char ev2citrusleaf_bin_name[32];

//
// An object is the value in a bin, or it is used as a key
// The object is typed according to the citrusleaf typing system
// These are often stack allocated, and are assigned using the 'wrap' calls
//

typedef struct ev2citrusleaf_object_s {
	enum ev2citrusleaf_type    type;
	size_t			size;
	union {
		char 		*str; // note for str: size is strlen (not strlen+1 
		void 		*blob;
		int64_t		i64;   // easiest to have one large int type
	} u;

	void *free; // if this is set, this must be freed on destructuion	

} ev2citrusleaf_object;

// A bin is a name and an object

typedef struct ev2citrusleaf_bin_s {
	ev2citrusleaf_bin_name		bin_name;
	ev2citrusleaf_object			object;
} ev2citrusleaf_bin;

enum ev2citrusleaf_operation_type { CL_OP_WRITE, CL_OP_READ, CL_OP_ADD };

typedef struct ev2citrusleaf_operation_s {
	ev2citrusleaf_bin_name		bin_name;
	enum ev2citrusleaf_operation_type op;
	ev2citrusleaf_object		object;
} ev2citrusleaf_operation;

//
// All citrusleaf functions return an integer. This integer is 0 if the
// call has succeeded, and a negative number if it has failed.
// All returns of pointers and objects are done through the parameters.
// (When in C++, use & parameters for return, but we're not there yet)
//
// 'void' return functions are only used for functions that are syntactically
// unable to fail.
//

//
// ev2citrusleaf_object calls
// 

// fill out the object structure with the string in question - no allocs
void ev2citrusleaf_object_init(ev2citrusleaf_object *o);
void ev2citrusleaf_object_set_null(ev2citrusleaf_object *o);
void ev2citrusleaf_object_init_str(ev2citrusleaf_object *o, char *str);
void ev2citrusleaf_object_init_str2(ev2citrusleaf_object *o, char *str, size_t buf_len);
void ev2citrusleaf_object_dup_str(ev2citrusleaf_object *o, char *str);
void ev2citrusleaf_object_init_blob(ev2citrusleaf_object *o, void *buf, size_t buf_len);
void ev2citrusleaf_object_init_blob2(enum ev2citrusleaf_type btype,ev2citrusleaf_object *o, void *buf, size_t buf_len);
void ev2citrusleaf_object_dup_blob(ev2citrusleaf_object *o, void *buf, size_t buf_len);
void ev2citrusleaf_object_dup_blob2(enum ev2citrusleaf_type btype, ev2citrusleaf_object *o, void *buf, size_t buf_len);
void ev2citrusleaf_object_init_int(ev2citrusleaf_object *o, int64_t i);
void ev2citrusleaf_object_free(ev2citrusleaf_object *o); 
void ev2citrusleaf_bins_free(ev2citrusleaf_bin *bins, int n_bins);



// use:
// Calling this will return an cl_conn, which is a structure internal to the library.
// You can use this to do multiple calls to Aerospike instead of returning
// and re-getting each time
// although if you have an error, it might be because that particular cluster
// went down and you might need to get a new connection to the cluster.
// (perhaps we'll hide that detail in future APIs)
//
// You will be required to return your connections! That would be called a leak.
//
// the host-port combo should be some way to get to the cluster (often a DNS name)
// However, it might be done. Using the cluster name allows the clustering
// internals to keep track of other host-ports which are the same cluster,
// and match your request to other hosts

/*
** when your result comes back,
** the 'bins' field will be allocated for you as a single block.
** you'll have to free it. Because, let's face it, we need to allocate out some
** space like this anyway, and we'll try to do that with only one alloc, and you
** might as well be able to enqueue the answer and send it somewhere else.
*/

typedef void (*ev2citrusleaf_callback) (int return_value,  ev2citrusleaf_bin *bins, int n_bins, uint32_t generation, void *udata );


// Caller may replace client library's mutex calls with these callbacks (e.g. to
// include them in an application monitoring scheme). To use this feature, pass
// a valid ev2citrusleaf_lock_callbacks pointer in ev2citrusleaf_init(). To let
// the client library do its own mutex calls, pass null in ev2citrusleaf_init().
//
// As defined in cf_base/include/citrusleaf/cf_hooks.h:
//
//	typedef struct cf_mutex_hooks_s {
//		// Allocate and initialize new lock.
//		void *(*alloc)(void);
//		// Release all storage held in 'lock'.
//		void (*free)(void *lock);
//		// Acquire an already-allocated lock at 'lock'.
//		int (*lock)(void *lock);
//		// Release a lock at 'lock'.
//		int (*unlock)(void *lock);
//	} cf_mutex_hooks;

typedef cf_mutex_hooks ev2citrusleaf_lock_callbacks;


/**
  Initialize the asynchronous Citrusleaf library
*/
int ev2citrusleaf_init(ev2citrusleaf_lock_callbacks *lock_cb);

void ev2citrusleaf_shutdown(bool fail_requests);

//
// This call will print stats to stderr
//
void ev2citrusleaf_print_stats(void);


/**
 * Create a cluster object - all requests are made on a cluster
 */

struct ev2citrusleaf_cluster_s;
typedef struct ev2citrusleaf_cluster_s ev2citrusleaf_cluster;

// Creates an event_base and thread internally, for maintenance and monitoring.
ev2citrusleaf_cluster *ev2citrusleaf_cluster_create();

// Before calling ev2citrusleaf_cluster_destroy(), stop initiating transaction
// requests to this cluster, and make sure that all in-progress transactions are
// completed, i.e. their callbacks have been made. One way to satisfy this
// requirement is to stop initiating transaction requests, then call
// ev2citrusleaf_cluster_destroy() with a delay_ms safely in excess of any
// timeout_ms value passed in requests.
//
// This call will block for delay_ms milliseconds.
void ev2citrusleaf_cluster_destroy(ev2citrusleaf_cluster *asc, int delay_ms);

// Adding a host to the cluster list which will always be checked for membership
// As this entire interface is async, the number of hosts in the cluster must be
// checked with a different, non-blocking, call
int ev2citrusleaf_cluster_add_host(ev2citrusleaf_cluster *cl, char *host, short port);

// Following is the act of tracking the cluster members as there are changes in
// ownership of the cluster, and load balancing. Following is enabled by default,
// turn it off only for debugging purposes
void ev2citrusleaf_cluster_follow(ev2citrusleaf_cluster *cl, bool flag);

// Gets the number of active nodes
// -1 means the call failed - the cluster object is invalid
// 0 means no nodes - won't get fast response
// more is good!
//
// Warning!  A typical code pattern would be to create the cluster, add a host,
// and loop on this call. That will never succeed, because libevent doesn't
// have an active thread. You will need to give libevent a thread, which is shown
// in the example distributed with this client. Or don't use threads and just
// dispatch.
int ev2citrusleaf_cluster_get_active_node_count(ev2citrusleaf_cluster *cl);

// Returns the number of requests in progress.
// May use this to check that all requests on a cluster are cleared before
// calling ev2citrusleaf_cluster_destroy().
int ev2citrusleaf_cluster_requests_in_progress(ev2citrusleaf_cluster *cl);


//
// An extended information structure
// when you want to control every little bit of write information you can
//
// Expiration is in *seconds from now*.
//
typedef struct {
	bool	use_generation;
	uint32_t generation;
	uint32_t expiration;
	ev2citrusleaf_write_policy wpol;
} ev2citrusleaf_write_parameters;

// If you'd like to start out with default parameters, call this function
static inline void ev2citrusleaf_write_parameters_init( ev2citrusleaf_write_parameters *wparam)
{
	wparam->use_generation = false; // ignore the following generation count
//	wparam->generation = 0;
	wparam->expiration = 0; // no per-item expiration
	wparam->wpol = CL_WRITE_RETRY;
}

//
// Get and put calls
//

int 
ev2citrusleaf_get_all(ev2citrusleaf_cluster *cl, char *ns, char *set, ev2citrusleaf_object *key, 
	int timeout_ms, ev2citrusleaf_callback cb, void *udata, struct event_base *base);

int
ev2citrusleaf_get_all_digest(ev2citrusleaf_cluster *cl, char *ns, cf_digest *d, int timeout_ms, 
	ev2citrusleaf_callback cb, void *udata, struct event_base *base);

int 
ev2citrusleaf_put(ev2citrusleaf_cluster *cl, char *ns, char *set, ev2citrusleaf_object *key,
	ev2citrusleaf_bin *bins, int n_bins, ev2citrusleaf_write_parameters *wparam, 
	int timeout_ms, ev2citrusleaf_callback cb, void *udata, struct event_base *base);

int 
ev2citrusleaf_put_digest(ev2citrusleaf_cluster *cl, char *ns, cf_digest *d,
	ev2citrusleaf_bin *bins, int n_bins, ev2citrusleaf_write_parameters *wparam, 
	int timeout_ms, ev2citrusleaf_callback cb, void *udata, struct event_base *base);

int 
ev2citrusleaf_get(ev2citrusleaf_cluster *cl, char *ns, char *set, ev2citrusleaf_object *key,
	const char **bins, int n_bins, int timeout_ms, ev2citrusleaf_callback cb, void *udata,
	struct event_base *base);

int 
ev2citrusleaf_get_digest(ev2citrusleaf_cluster *cl, char *ns, cf_digest *d,
	const char **bins, int n_bins, int timeout_ms, ev2citrusleaf_callback cb, void *udata,
	struct event_base *base);

int 
ev2citrusleaf_delete(ev2citrusleaf_cluster *cl, char *ns, char *set, ev2citrusleaf_object *key,
	ev2citrusleaf_write_parameters *wparam, int timeout_ms, ev2citrusleaf_callback cb, void *udata,
	struct event_base *base);

int 
ev2citrusleaf_delete_digest(ev2citrusleaf_cluster *cl, char *ns, cf_digest *d,
	ev2citrusleaf_write_parameters *wparam, int timeout_ms, ev2citrusleaf_callback cb, void *udata,
	struct event_base *base);

int
ev2citrusleaf_operate(ev2citrusleaf_cluster *cl, char *ns, char *set, ev2citrusleaf_object *key,
	ev2citrusleaf_operation *ops, int n_ops, ev2citrusleaf_write_parameters *wparam, 
	int timeout_ms, ev2citrusleaf_callback cb, void *udata, struct event_base *base);

int
ev2citrusleaf_operate_digest(ev2citrusleaf_cluster *cl, char *ns, cf_digest *d,
	ev2citrusleaf_operation *ops, int n_ops, ev2citrusleaf_write_parameters *wparam, 
	int timeout_ms, ev2citrusleaf_callback cb, void *udata, struct event_base *base);


//
// the info interface allows
// information about specific cluster features to be retrieved on a host by host basis
// size_t is in number of bytes. String is null terminated as well
// API CONTRACT: *callee* frees the 'response' buffer
typedef void (*ev2citrusleaf_info_callback) (int return_value, char *response, size_t response_len, void *udata);

int
ev2citrusleaf_info(struct event_base *base, struct evdns_base *dns_base, char *host, short port, char *names, int timeout_ms,
	ev2citrusleaf_info_callback cb, void *udata);

//
// This debugging call can be useful for tracking down errors and coordinating with server failures
//
int
ev2citrusleaf_calculate_digest(const char *set, const ev2citrusleaf_object *key, cf_digest *digest);

//
// Logging - see cf_log.h
//

#ifdef __cplusplus
} // end extern "C"
#endif


#endif

