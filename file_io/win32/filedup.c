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

#include "fileio.h"
#include "apr_file_io.h"
#include "apr_general.h"
#include "apr_strings.h"
#include <string.h>
#if APR_HAS_UNICODE_FS
#include "i18n.h"
#include <wchar.h>
#endif

apr_status_t apr_dupfile(apr_file_t **new_file, apr_file_t *old_file, apr_pool_t *p)
{
    BOOLEAN isStdHandle = FALSE;
    HANDLE hCurrentProcess = GetCurrentProcess();
    apr_oslevel_e os_level;

    if ((*new_file) == NULL) {
        if (p == NULL) {
            p = old_file->cntxt;
        }
            
        (*new_file) = (apr_file_t *) apr_pcalloc(p, sizeof(apr_file_t));
        if ((*new_file) == NULL) {
            return APR_ENOMEM;
        }
        if (!DuplicateHandle(hCurrentProcess, old_file->filehand, 
                             hCurrentProcess,
                             &(*new_file)->filehand, 0, FALSE, 
                             DUPLICATE_SAME_ACCESS)) {
            return apr_get_os_error();
        }
    } else {
        HANDLE hFile = (*new_file)->filehand;
        /* dup2 is not supported with native Windows handles. We 
         * can, however, emulate dup2 for the standard i/o handles.
         */
        if (hFile == GetStdHandle(STD_ERROR_HANDLE)) {
            isStdHandle = TRUE;
            if (!SetStdHandle(STD_ERROR_HANDLE, old_file->filehand))
                return apr_get_os_error();
        }
        else if (hFile == GetStdHandle(STD_OUTPUT_HANDLE)) {
            isStdHandle = TRUE;
            if (!SetStdHandle(STD_OUTPUT_HANDLE, old_file->filehand))
                return apr_get_os_error();
        }
        else if (hFile == GetStdHandle(STD_INPUT_HANDLE)) {
            isStdHandle = TRUE;
            if (!SetStdHandle(STD_INPUT_HANDLE, old_file->filehand))
                return apr_get_os_error();
        }
        else
            return APR_ENOTIMPL;
    }

    (*new_file)->cntxt = old_file->cntxt;
#if APR_HAS_UNICODE_FS
    if (!apr_get_oslevel(old_file->cntxt, &os_level) && os_level >= APR_WIN_NT)
    {
        int len = wcslen(old_file->w.fname) + 1;
        (*new_file)->w.fname = apr_palloc(old_file->cntxt, len * 2);
        wcscpy((*new_file)->w.fname, old_file->w.fname);
    }
    else
#endif
        (*new_file)->n.fname = apr_pstrdup(old_file->cntxt, old_file->n.fname);

/*    (*new_file)->demonfname = apr_pstrdup(old_file->cntxt, old_file->demonfname);
 *    (*new_file)->lowerdemonfname = apr_pstrdup(old_file->cntxt, old_file->lowerdemonfname);
 */
    (*new_file)->append = old_file->append;
/*    (*new_file)->protection = old_file->protection;
 *    (*new_file)->user = old_file->user;
 *    (*new_file)->group = old_file->group;
 */
    (*new_file)->size = old_file->size;
    (*new_file)->atime = old_file->atime;    
    (*new_file)->mtime = old_file->mtime;
    (*new_file)->ctime = old_file->ctime;
    (*new_file)->buffered = FALSE;

    if (!isStdHandle) {
        apr_register_cleanup((*new_file)->cntxt, (void *)(*new_file), file_cleanup,
                            apr_null_cleanup);
    }

    return APR_SUCCESS;
}


