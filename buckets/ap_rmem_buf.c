/* ====================================================================
 * Copyright (c) 1996-1999 The Apache Group.  All rights reserved.
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
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

#include "apr_private.h"
#include "apr_buf.h"
#include <stdlib.h>

#ifndef DEFAULT_RWBUF_SIZE
#define DEFAULT_RWBUF_SIZE (4096)
#endif

static const char * rmem_get_str(ap_bucket *e)
{
    ap_bucket_rmem *b = (ap_bucket_rmem *)e->data;
    return b->start;
}

static int rmem_get_len(ap_bucket *e)
{
    ap_bucket_rmem *b = (ap_bucket_rmem *)e->data;
    return (char *)b->end - (char *)b->start;
}

APR_EXPORT(ap_bucket *) ap_rmem_create(void)
{
    ap_bucket *newbuf;
    ap_bucket_rmem *b;

    newbuf                = malloc(sizeof(*newbuf));
    b                     = malloc(sizeof(*b)); 

    b->alloc_len          = 0;
    b->start = b->end     = NULL;

    newbuf->color         = AP_BUCKET_rmem;
    newbuf->getstr        = rmem_get_str;
    newbuf->getlen        = rmem_get_len;
    newbuf->free          = NULL;
    newbuf->data          = b;
    return newbuf;
}

/*
 * save nbyte bytes to the bucket.
 * Only returns fewer than nbyte if an error ocurred.
 * Returns -1 if no bytes were written before the error ocurred.
 * It is worth noting that if an error occurs, the buffer is in an unknown
 * state.
 */
APR_EXPORT(int) ap_rmem_write(ap_bucket_rmem *b, const void *buf,
                               ap_size_t nbyte, ap_ssize_t *bytes_written)
{
    if (nbyte == 0) {
        *bytes_written = 0;
        return APR_SUCCESS;
    }

    /* We should probably do some checking to make sure we don't allocate too
     * much memory, but that can wait for the second pass.
     */
    b->start = buf;
    b->end = (char *)b->start + nbyte;
    *bytes_written = nbyte;
    return APR_SUCCESS;
}

