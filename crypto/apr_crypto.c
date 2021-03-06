/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <stdio.h>

#include "apu.h"
#include "apr_private.h"
#include "apr_pools.h"
#include "apr_dso.h"
#include "apr_strings.h"
#include "apr_hash.h"
#include "apr_thread_mutex.h"
#include "apr_lib.h"

#if APU_HAVE_CRYPTO

#include "apu_internal.h"
#include "apr_crypto_internal.h"
#include "apr_crypto.h"
#include "apr_version.h"

static apr_hash_t *drivers = NULL;

#define ERROR_SIZE 1024

#define CLEANUP_CAST (apr_status_t (*)(void*))

APR_TYPEDEF_STRUCT(apr_crypto_t, \
   apr_pool_t *pool; \
   apr_crypto_driver_t *provider; \
)

APR_TYPEDEF_STRUCT(apr_crypto_key_t, \
   apr_pool_t *pool; \
   apr_crypto_driver_t *provider; \
   const apr_crypto_t *f;
)

APR_TYPEDEF_STRUCT(apr_crypto_block_t, \
   apr_pool_t *pool; \
   apr_crypto_driver_t *provider; \
   const apr_crypto_t *f;
)

#if !APR_HAVE_MODULAR_DSO
#define DRIVER_LOAD(name,driver,pool,params) \
    {   \
        extern const apr_crypto_driver_t driver; \
        apr_hash_set(drivers,name,APR_HASH_KEY_STRING,&driver); \
        if (driver.init) {     \
            driver.init(pool, params); \
        }  \
    }
#endif

static apr_status_t apr_crypto_term(void *ptr) {
    /* set drivers to NULL so init can work again */
    drivers = NULL;

    /* Everything else we need is handled by cleanups registered
     * when we created mutexes and loaded DSOs
     */
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_crypto_init(apr_pool_t *pool) {
    apr_status_t ret = APR_SUCCESS;
    apr_pool_t *parent;

    if (drivers != NULL) {
        return APR_SUCCESS;
    }

    /* Top level pool scope, need process-scope lifetime */
    for (parent = pool; parent; parent = apr_pool_parent_get(pool))
        pool = parent;
#if APR_HAVE_MODULAR_DSO
    /* deprecate in 2.0 - permit implicit initialization */
    apu_dso_init(pool);
#endif
    drivers = apr_hash_make(pool);

#if !APR_HAVE_MODULAR_DSO
    /* Load statically-linked drivers: */
#if APU_HAVE_OPENSSL
    DRIVER_LOAD("openssl", apr_crypto_openssl_driver, pool, params);
#endif
#if APU_HAVE_NSS
    DRIVER_LOAD("nss", apr_crypto_nss_driver, pool, params);
#endif
#if APU_HAVE_MSCAPI
    DRIVER_LOAD("mscapi", apr_crypto_mscapi_driver, pool, params);
#endif
#if APU_HAVE_MSCNG
    DRIVER_LOAD("mscng", apr_crypto_mscng_driver, pool, params);
#endif
#endif /* APR_HAVE_MODULAR_DSO */

    apr_pool_cleanup_register(pool, NULL, apr_crypto_term,
            apr_pool_cleanup_null);

    return ret;
}

APR_DECLARE(apr_status_t) apr_crypto_get_driver(const apr_crypto_driver_t **driver,
        const char *name, const apr_array_header_t *params, const apu_err_t **result,
        apr_pool_t *pool) {
#if APR_HAVE_MODULAR_DSO
    char modname[32];
    char symname[34];
    apr_dso_handle_t *dso;
    apr_dso_handle_sym_t symbol;
#endif
    apr_status_t rv;
    int rc = 0;

#if APR_HAVE_MODULAR_DSO
    rv = apu_dso_mutex_lock();
    if (rv) {
        return rv;
    }
#endif
    *driver = apr_hash_get(drivers, name, APR_HASH_KEY_STRING);
    if (*driver) {
#if APR_HAVE_MODULAR_DSO
        apu_dso_mutex_unlock();
#endif
        return APR_SUCCESS;
    }

#if APR_HAVE_MODULAR_DSO
    /* The driver DSO must have exactly the same lifetime as the
     * drivers hash table; ignore the passed-in pool */
    pool = apr_hash_pool_get(drivers);

#if defined(NETWARE)
    apr_snprintf(modname, sizeof(modname), "crypto%s.nlm", name);
#elif defined(WIN32)
    apr_snprintf(modname, sizeof(modname),
            "apr_crypto_%s-" APR_STRINGIFY(APR_MAJOR_VERSION) ".dll", name);
#else
    apr_snprintf(modname, sizeof(modname), "apr_crypto_%s-" APR_STRINGIFY(APR_MAJOR_VERSION) ".so", name);
#endif
    apr_snprintf(symname, sizeof(symname), "apr_crypto_%s_driver", name);
    rv = apu_dso_load(&dso, &symbol, modname, symname, pool);
    if (rv != APR_SUCCESS) { /* APR_EDSOOPEN or APR_ESYMNOTFOUND? */
        if (rv == APR_EINIT) { /* previously loaded?!? */
            name = apr_pstrdup(pool, name);
            apr_hash_set(drivers, name, APR_HASH_KEY_STRING, *driver);
            rv = APR_SUCCESS;
        }
        goto unlock;
    }
    *driver = symbol;
    if ((*driver)->init) {
        rv = (*driver)->init(pool, params, &rc);
    }
    name = apr_pstrdup(pool, name);
    apr_hash_set(drivers, name, APR_HASH_KEY_STRING, *driver);

    unlock: apu_dso_mutex_unlock();

    if (APR_SUCCESS != rv && result) {
        char *buffer = apr_pcalloc(pool, ERROR_SIZE);
        apu_err_t *err = apr_pcalloc(pool, sizeof(apu_err_t));
        if (err && buffer) {
            apr_dso_error(dso, buffer, ERROR_SIZE - 1);
            err->msg = buffer;
            err->reason = modname;
            err->rc = rc;
            *result = err;
        }
    }

#else /* not builtin and !APR_HAS_DSO => not implemented */
    rv = APR_ENOTIMPL;
#endif

    return rv;
}

/**
 * @brief Return the name of the driver.
 *
 * @param driver - The driver in use.
 * @return The name of the driver.
 */
APR_DECLARE(const char *)apr_crypto_driver_name (const apr_crypto_driver_t *driver)
{
    return driver->name;
}

/**
 * @brief Get the result of the last operation on a context. If the result
 *        is NULL, the operation was successful.
 * @param result - the result structure
 * @param f - context pointer
 * @return APR_SUCCESS for success
 */
APR_DECLARE(apr_status_t) apr_crypto_error(const apu_err_t **result,
        const apr_crypto_t *f) {
    return f->provider->error(result, f);
}

/**
 * @brief Create a context for supporting encryption. Keys, certificates,
 *        algorithms and other parameters will be set per context. More than
 *        one context can be created at one time. A cleanup will be automatically
 *        registered with the given pool to guarantee a graceful shutdown.
 * @param f - context pointer will be written here
 * @param driver - driver to use
 * @param params - array of key parameters
 * @param pool - process pool
 * @return APR_ENOENGINE when the engine specified does not exist. APR_EINITENGINE
 * if the engine cannot be initialised.
 */
APR_DECLARE(apr_status_t) apr_crypto_make(apr_crypto_t **f, const apr_crypto_driver_t *driver,
        const apr_array_header_t *params, apr_pool_t *pool) {
    return driver->make(f, driver, params, pool);
}

/**
 * @brief Get a hash table of key types, keyed by the name of the type against
 * an integer pointer constant.
 *
 * @param types - hashtable of key types keyed to constants.
 * @param f - encryption context
 * @return APR_SUCCESS for success
 */
APR_DECLARE(apr_status_t) apr_crypto_get_block_key_types(apr_hash_t **types,
        const apr_crypto_t *f)
{
    return f->provider->get_block_key_types(types, f);
}

/**
 * @brief Get a hash table of key modes, keyed by the name of the mode against
 * an integer pointer constant.
 *
 * @param modes - hashtable of key modes keyed to constants.
 * @param f - encryption context
 * @return APR_SUCCESS for success
 */
APR_DECLARE(apr_status_t) apr_crypto_get_block_key_modes(apr_hash_t **modes,
        const apr_crypto_t *f)
{
    return f->provider->get_block_key_modes(modes, f);
}

/**
 * @brief Create a key from the given passphrase. By default, the PBKDF2
 *        algorithm is used to generate the key from the passphrase. It is expected
 *        that the same pass phrase will generate the same key, regardless of the
 *        backend crypto platform used. The key is cleaned up when the context
 *        is cleaned, and may be reused with multiple encryption or decryption
 *        operations.
 * @note If *key is NULL, a apr_crypto_key_t will be created from a pool. If
 *       *key is not NULL, *key must point at a previously created structure.
 * @param key The key returned, see note.
 * @param ivSize The size of the initialisation vector will be returned, based
 *               on whether an IV is relevant for this type of crypto.
 * @param pass The passphrase to use.
 * @param passLen The passphrase length in bytes
 * @param salt The salt to use.
 * @param saltLen The salt length in bytes
 * @param type 3DES_192, AES_128, AES_192, AES_256.
 * @param mode Electronic Code Book / Cipher Block Chaining.
 * @param doPad Pad if necessary.
 * @param iterations Number of iterations to use in algorithm
 * @param f The context to use.
 * @param p The pool to use.
 * @return Returns APR_ENOKEY if the pass phrase is missing or empty, or if a backend
 *         error occurred while generating the key. APR_ENOCIPHER if the type or mode
 *         is not supported by the particular backend. APR_EKEYTYPE if the key type is
 *         not known. APR_EPADDING if padding was requested but is not supported.
 *         APR_ENOTIMPL if not implemented.
 */
APR_DECLARE(apr_status_t) apr_crypto_passphrase(apr_crypto_key_t **key,
        apr_size_t *ivSize, const char *pass, apr_size_t passLen,
        const unsigned char * salt, apr_size_t saltLen,
        const apr_crypto_block_key_type_e type,
        const apr_crypto_block_key_mode_e mode, const int doPad,
        const int iterations, const apr_crypto_t *f, apr_pool_t *p)
{
    return f->provider->passphrase(key, ivSize, pass, passLen, salt, saltLen,
            type, mode, doPad, iterations, f, p);
}

/**
 * @brief Initialise a context for encrypting arbitrary data using the given key.
 * @note If *ctx is NULL, a apr_crypto_block_t will be created from a pool. If
 *       *ctx is not NULL, *ctx must point at a previously created structure.
 * @param ctx The block context returned, see note.
 * @param iv Optional initialisation vector. If the buffer pointed to is NULL,
 *           an IV will be created at random, in space allocated from the pool.
 *           If the buffer pointed to is not NULL, the IV in the buffer will be
 *           used.
 * @param key The key structure to use.
 * @param blockSize The block size of the cipher.
 * @param p The pool to use.
 * @return Returns APR_ENOIV if an initialisation vector is required but not specified.
 *         Returns APR_EINIT if the backend failed to initialise the context. Returns
 *         APR_ENOTIMPL if not implemented.
 */
APR_DECLARE(apr_status_t) apr_crypto_block_encrypt_init(
        apr_crypto_block_t **ctx, const unsigned char **iv,
        const apr_crypto_key_t *key, apr_size_t *blockSize, apr_pool_t *p)
{
    return key->provider->block_encrypt_init(ctx, iv, key, blockSize, p);
}

/**
 * @brief Encrypt data provided by in, write it to out.
 * @note The number of bytes written will be written to outlen. If
 *       out is NULL, outlen will contain the maximum size of the
 *       buffer needed to hold the data, including any data
 *       generated by apr_crypto_block_encrypt_finish below. If *out points
 *       to NULL, a buffer sufficiently large will be created from
 *       the pool provided. If *out points to a not-NULL value, this
 *       value will be used as a buffer instead.
 * @param out Address of a buffer to which data will be written,
 *        see note.
 * @param outlen Length of the output will be written here.
 * @param in Address of the buffer to read.
 * @param inlen Length of the buffer to read.
 * @param ctx The block context to use.
 * @return APR_ECRYPT if an error occurred. Returns APR_ENOTIMPL if
 *         not implemented.
 */
APR_DECLARE(apr_status_t) apr_crypto_block_encrypt(unsigned char **out,
        apr_size_t *outlen, const unsigned char *in, apr_size_t inlen,
        apr_crypto_block_t *ctx)
{
    return ctx->provider->block_encrypt(out, outlen, in, inlen, ctx);
}

/**
 * @brief Encrypt final data block, write it to out.
 * @note If necessary the final block will be written out after being
 *       padded. Typically the final block will be written to the
 *       same buffer used by apr_crypto_block_encrypt, offset by the
 *       number of bytes returned as actually written by the
 *       apr_crypto_block_encrypt() call. After this call, the context
 *       is cleaned and can be reused by apr_crypto_block_encrypt_init().
 * @param out Address of a buffer to which data will be written. This
 *            buffer must already exist, and is usually the same
 *            buffer used by apr_evp_crypt(). See note.
 * @param outlen Length of the output will be written here.
 * @param ctx The block context to use.
 * @return APR_ECRYPT if an error occurred.
 * @return APR_EPADDING if padding was enabled and the block was incorrectly
 *         formatted.
 * @return APR_ENOTIMPL if not implemented.
 */
APR_DECLARE(apr_status_t) apr_crypto_block_encrypt_finish(unsigned char *out,
        apr_size_t *outlen, apr_crypto_block_t *ctx)
{
    return ctx->provider->block_encrypt_finish(out, outlen, ctx);
}

/**
 * @brief Initialise a context for decrypting arbitrary data using the given key.
 * @note If *ctx is NULL, a apr_crypto_block_t will be created from a pool. If
 *       *ctx is not NULL, *ctx must point at a previously created structure.
 * @param ctx The block context returned, see note.
 * @param blockSize The block size of the cipher.
 * @param iv Optional initialisation vector.
 * @param key The key structure to use.
 * @param p The pool to use.
 * @return Returns APR_ENOIV if an initialisation vector is required but not specified.
 *         Returns APR_EINIT if the backend failed to initialise the context. Returns
 *         APR_ENOTIMPL if not implemented.
 */
APR_DECLARE(apr_status_t) apr_crypto_block_decrypt_init(
        apr_crypto_block_t **ctx, apr_size_t *blockSize,
        const unsigned char *iv, const apr_crypto_key_t *key, apr_pool_t *p)
{
    return key->provider->block_decrypt_init(ctx, blockSize, iv, key, p);
}

/**
 * @brief Decrypt data provided by in, write it to out.
 * @note The number of bytes written will be written to outlen. If
 *       out is NULL, outlen will contain the maximum size of the
 *       buffer needed to hold the data, including any data
 *       generated by apr_crypto_block_decrypt_finish below. If *out points
 *       to NULL, a buffer sufficiently large will be created from
 *       the pool provided. If *out points to a not-NULL value, this
 *       value will be used as a buffer instead.
 * @param out Address of a buffer to which data will be written,
 *        see note.
 * @param outlen Length of the output will be written here.
 * @param in Address of the buffer to read.
 * @param inlen Length of the buffer to read.
 * @param ctx The block context to use.
 * @return APR_ECRYPT if an error occurred. Returns APR_ENOTIMPL if
 *         not implemented.
 */
APR_DECLARE(apr_status_t) apr_crypto_block_decrypt(unsigned char **out,
        apr_size_t *outlen, const unsigned char *in, apr_size_t inlen,
        apr_crypto_block_t *ctx)
{
    return ctx->provider->block_decrypt(out, outlen, in, inlen, ctx);
}

/**
 * @brief Decrypt final data block, write it to out.
 * @note If necessary the final block will be written out after being
 *       padded. Typically the final block will be written to the
 *       same buffer used by apr_crypto_block_decrypt, offset by the
 *       number of bytes returned as actually written by the
 *       apr_crypto_block_decrypt() call. After this call, the context
 *       is cleaned and can be reused by apr_crypto_block_decrypt_init().
 * @param out Address of a buffer to which data will be written. This
 *            buffer must already exist, and is usually the same
 *            buffer used by apr_evp_crypt(). See note.
 * @param outlen Length of the output will be written here.
 * @param ctx The block context to use.
 * @return APR_ECRYPT if an error occurred.
 * @return APR_EPADDING if padding was enabled and the block was incorrectly
 *         formatted.
 * @return APR_ENOTIMPL if not implemented.
 */
APR_DECLARE(apr_status_t) apr_crypto_block_decrypt_finish(unsigned char *out,
        apr_size_t *outlen, apr_crypto_block_t *ctx)
{
    return ctx->provider->block_decrypt_finish(out, outlen, ctx);
}

/**
 * @brief Clean encryption / decryption context.
 * @note After cleanup, a context is free to be reused if necessary.
 * @param ctx The block context to use.
 * @return Returns APR_ENOTIMPL if not supported.
 */
APR_DECLARE(apr_status_t) apr_crypto_block_cleanup(apr_crypto_block_t *ctx)
{
    return ctx->provider->block_cleanup(ctx);
}

/**
 * @brief Clean encryption / decryption context.
 * @note After cleanup, a context is free to be reused if necessary.
 * @param f The context to use.
 * @return Returns APR_ENOTIMPL if not supported.
 */
APR_DECLARE(apr_status_t) apr_crypto_cleanup(apr_crypto_t *f)
{
    return f->provider->cleanup(f);
}

/**
 * @brief Shutdown the crypto library.
 * @note After shutdown, it is expected that the init function can be called again.
 * @param driver - driver to use
 * @return Returns APR_ENOTIMPL if not supported.
 */
APR_DECLARE(apr_status_t) apr_crypto_shutdown(const apr_crypto_driver_t *driver)
{
    return driver->shutdown();
}

#endif /* APU_HAVE_CRYPTO */
