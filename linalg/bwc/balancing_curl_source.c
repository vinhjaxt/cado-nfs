#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "balancing_curl_source.h"
#include "balancing_rollbuf.h"

static int curl_global_init_done;
static pthread_mutex_t curl_global_init_done_mx = PTHREAD_MUTEX_INITIALIZER;

typedef size_t (*curl_writefunction_callback_t)(void *, size_t, size_t, void *);

static size_t
curl_source_callback(void *ptr, size_t size, size_t nmemb, rollbuf_ptr r)
{
    size_t realsize = size * nmemb;

    size_t rc = rollbuf_put(r, ptr, realsize);
    assert(rc == 0 || rc == realsize);
    return rc;
}


static void * curl_source_producer_thread(void * data)
{
    curl_source_ptr cs = (curl_source_ptr)data;
    curl_easy_perform(cs->curl_handle);
    rollbuf_mark_done(cs->r);
    return NULL;
}

data_source_ptr curl_source_alloc(const char * uri, size_t esz __attribute__((unused)))
{
    curl_source_ptr p = malloc(sizeof(curl_source));
    memset(p, 0, sizeof(curl_source));
    p->uri = uri;

    pthread_mutex_lock(&curl_global_init_done_mx);
    if (!curl_global_init_done++)
        curl_global_init(CURL_GLOBAL_ALL);
    pthread_mutex_unlock(&curl_global_init_done_mx);

    p->curl_handle = curl_easy_init();

    rollbuf_init(p->r);

    curl_easy_setopt(p->curl_handle, CURLOPT_URL, p->uri);
    curl_easy_setopt(p->curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    curl_easy_setopt(p->curl_handle, CURLOPT_WRITEFUNCTION, (curl_writefunction_callback_t) &curl_source_callback);
    curl_easy_setopt(p->curl_handle, CURLOPT_WRITEDATA, (void *)p->r);

    pthread_create(&p->producer, NULL, curl_source_producer_thread, p);

    p->b->get = (size_t(*)(void*,uint32_t **,size_t)) curl_source_get;
    return (data_source_ptr) p;
}

void curl_source_free(data_source_ptr q)
{
    curl_source_ptr p = (curl_source_ptr) q;

    pthread_join(p->producer, NULL);
    rollbuf_clear(p->r);
    curl_easy_cleanup(p->curl_handle);

    pthread_mutex_lock(&curl_global_init_done_mx);
    if (!--curl_global_init_done)
        curl_global_cleanup();
    pthread_mutex_unlock(&curl_global_init_done_mx);
}

size_t curl_source_get(curl_source_ptr f, void ** p, size_t avail)
{
    /* The API in the balancing code talks abount uint32_t * areas, with
     * size counted in uint32_t's. Here we are talking bytes.
     */
    assert((*p == NULL) == (avail == 0));

    /* Again, convert to uint32_t's if deemed necessary ! XXX */
    int rc = rollbuf_get2(f->r, p, avail);
    if (rc == 0)
        assert(rollbuf_is_done(f->r));
    if (!rollbuf_is_done(f->r))
        assert(rc % sizeof(uint32_t) == 0);
    size_t n = rc / sizeof(uint32_t);
    f->b->pos += n;
    return n;
}
