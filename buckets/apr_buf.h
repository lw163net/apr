/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 */

#ifndef AP_BUF_H
#define AP_BUF_H

#include "apr_mmap.h"
#include "apr_errno.h"
#include "../../../include/ap_iol.h"
#include "apr_private.h"
#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>	/* for struct iovec */
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

typedef enum {
    AP_BUCKET_rwmem,
    AP_BUCKET_rmem,
    AP_BUCKET_file,
    AP_BUCKET_mmap,
    AP_BUCKET_filename,
    AP_BUCKET_cached_entity,
    AP_BUCKET_URI,
    AP_BUCKET_eos        /* End-of-stream bucket.  Special case to say this is
                          * the end of the bucket so all data should be sent
                          * immediately. */
} ap_bucket_color_e;

typedef struct ap_bucket ap_bucket;
struct ap_bucket {
    ap_bucket_color_e color;              /* what type of bucket is it */
    void (*free)(void *e);                /* can be NULL */
    void *data;				  /* for use by free() */
    const char *(*getstr)(ap_bucket *e);  /* Get the string */
    int (*getlen)(ap_bucket *e);          /* Get the length of the string */
};

typedef struct ap_bucket_list ap_bucket_list;
struct ap_bucket_list {
    ap_bucket *bucket;                   /* The bucket */
    ap_bucket_list *next;                /* The next node in the bucket list */
    ap_bucket_list *prev;                /* The prev node in the bucket list */
};

typedef struct ap_bucket_brigade ap_bucket_brigade;
struct ap_bucket_brigade {
    ap_pool_t *p;                       /* The pool to associate this with.
                                           I do not allocate out of the pool,
                                           but this lets me register a cleanup
                                           to put a limit on the brigade's 
                                           lifetime. */
    ap_bucket_list *head;               /* The start of the brigade */
    ap_bucket_list *tail;               /* The end of the brigade */
};

/*   ******  Bucket Brigade Functions  *****  */

/* Create a new bucket brigade */
APR_EXPORT(ap_bucket_brigade *) ap_bucket_brigade_create(ap_pool_t *p);

/* destroy an enitre bucket brigade */
APR_EXPORT(ap_status_t) ap_bucket_brigade_destroy(void *b);

/* append a bucket_list to a bucket_brigade */
APR_EXPORT(void) ap_bucket_brigade_append_list(ap_bucket_brigade *b, 
                                               ap_bucket_list *e);

/* append a bucket to a bucket_brigade */
APR_EXPORT(void) ap_bucket_brigade_append_bucket(ap_bucket_brigade *b,
                                                 ap_bucket *r);

/* consume nbytes from beginning of b -- call ap_bucket_destroy as
    appropriate, and/or modify start on last element */
APR_EXPORT(void) ap_bucket_brigade_consume(ap_bucket_brigade *, int nbytes);

/* create an iovec of the elements in a bucket_brigade... return number 
    of elements used */
APR_EXPORT(int) ap_bucket_brigade_to_iovec(ap_bucket_brigade *, 
                                           struct iovec *vec, int nvec);

/* catenate bucket_brigade b onto bucket_brigade a, bucket_brigade b is 
    empty after this */
APR_EXPORT(void) ap_bucket_brigade_catenate(ap_bucket_brigade *a, 
                                            ap_bucket_brigade *b);

/* Destroy the first nvec buckets. */
APR_EXPORT(void) ap_consume_buckets(ap_bucket_brigade *b, int nvec);

/* save the buf out to the specified iol.  This can be used to flush the
    data to the disk, or to send it out to the network. */
APR_EXPORT(ap_status_t) ap_bucket_brigade_to_iol(ap_ssize_t *total_bytes,
                                                 ap_bucket_brigade *a, 
                                                 ap_iol *iol);

APR_EXPORT(int) ap_brigade_vputstrs(ap_bucket_brigade *b, va_list va);

APR_EXPORT(int) ap_brigade_printf(ap_bucket_brigade *b, const char *fmt, ...);

APR_EXPORT(int) ap_brigade_vprintf(ap_bucket_brigade *b, const char *fmt, va_list va);

/*   ******  Bucket List Functions  *****  */

/* create a new bucket_list */
APR_EXPORT(ap_bucket_list *) ap_bucket_list_create(void);

/* destroy an entire bucket_list */
APR_EXPORT(ap_status_t) ap_destroy_bucket_list(ap_bucket_list *b);

/*   ******  Bucket Functions  *****  */

/* allocate a bucket of type color */
APR_EXPORT(ap_bucket *) ap_bucket_new(ap_bucket_color_e color);

/* destroy a bucket */
APR_EXPORT(ap_status_t) ap_bucket_destroy(ap_bucket *e);

/* Convert a bucket to a char * */
APR_EXPORT(const char *) ap_get_bucket_char_str(ap_bucket *b);

/* get the length of the data in the bucket */
APR_EXPORT(int) ap_get_bucket_len(ap_bucket *b);

/*   ******  RWMEM Functions  *****  */

typedef struct ap_bucket_rwmem ap_bucket_rwmem;
struct ap_bucket_rwmem {
    void    *alloc_addr;                /* Where does the data start */
    size_t  alloc_len;                  /* how much was allocated */
    void    *start;                     /* Where does the actual data start
                                           in the alloc'ed block */
    void    *end;                       /* where does the data actually end? */
};

/* Create a read/write memory bucket */
APR_EXPORT(ap_bucket *) ap_rwmem_create(void);

APR_EXPORT(int) ap_rwmem_write(ap_bucket_rwmem *b, const void *buf,
                               ap_size_t nbyte, ap_ssize_t *bytes_written);

APR_EXPORT(int) ap_rwmem_vputstrs(ap_bucket_rwmem *b, va_list va);

/*   ******  MMAP Functions  *****  */

typedef struct ap_bucket_mmap ap_bucket_mmap;
struct ap_bucket_mmap {
    ap_mmap_t *data;
};

/* Create a mmap memory bucket */
APR_EXPORT(ap_bucket *) ap_mmap_bucket_create(void);

APR_EXPORT(void) ap_mmap_bucket_insert(ap_bucket_mmap *b, ap_mmap_t *mm);

/*   ******  RMEM Functions  *****  */

typedef struct ap_bucket_rmem ap_bucket_rmem;
struct ap_bucket_rmem {
    size_t  alloc_len;                  /* how much was allocated */
    const void    *start;               /* Where does the actual data start
                                           in the alloc'ed block */
    const void    *end;                 /* where does the data actually end? */
};

/* Create a read only memory bucket */
APR_EXPORT(ap_bucket *) ap_rmem_create(void);

APR_EXPORT(int) ap_rmem_write(ap_bucket_rmem *b, const void *buf,
                               ap_size_t nbyte, ap_ssize_t *bytes_written);

APR_EXPORT(int) ap_rmem_vputstrs(ap_bucket_rmem *b, va_list va);

/*   ******  RMEM Functions  *****  */

/* Create an End of Stream bucket */
APR_EXPORT(ap_bucket *) ap_eos_create(void);

#endif

