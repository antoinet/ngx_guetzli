/*
 * ngx_http_guetzli_filter_module
 *
 * (c) 2014 Antoine Neuenschwander <antoine@schoggi.org>
 */

#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <string.h>
#include <hiredis/hiredis.h>
#include <uuid/uuid.h>



/* module configuration structure */
typedef struct {
    ngx_str_t   redis_host;
    ngx_int_t   redis_port;
    ngx_str_t   cookie_name;
} ngx_http_guetzli_loc_conf_t;


/* representation of cookies */
typedef struct {
    ngx_str_t   name;
    time_t      expires_time;
    ngx_str_t   domain;
    ngx_str_t   path;
    ngx_http_complex_value_t* value;
} ngx_guetzli_cookie_t;


/* module functions */
static ngx_int_t ngx_http_guetzli_init(ngx_conf_t *cf);
static void* ngx_http_guetzli_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_guetzli_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child);
static ngx_int_t ngx_http_guetzli_filter(ngx_http_request_t *r);
static ngx_int_t ngx_http_guetzli_set_cookie(ngx_http_request_t *r, ngx_http_guetzli_loc_conf_t *conf, ngx_str_t *uuid); 
static ngx_int_t ngx_http_guetzli_get_cookie(ngx_http_request_t *r, ngx_http_guetzli_loc_conf_t *conf, ngx_str_t *uuid);
static ngx_int_t redis_lookup_uuid(ngx_http_guetzli_loc_conf_t *conf, ngx_str_t *uuid);
static ngx_int_t redis_store_uuid(ngx_http_guetzli_loc_conf_t *conf, ngx_str_t *uuid);
static ngx_int_t uuid_verify(ngx_str_t *in);
static ngx_int_t hex_decode(u_char *dst, u_char *src, size_t len);

/* definition of configuration directives */
static ngx_command_t ngx_http_guetzli_commands[] = {
    {
        ngx_string("guetzli_redis_host"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_guetzli_loc_conf_t, redis_host),
        NULL
    },
    {
        ngx_string("guetzli_redis_port"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_guetzli_loc_conf_t, redis_port),
        NULL
    },
    {
        ngx_string("guetzli_cookie_name"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_guetzli_loc_conf_t, cookie_name),
        NULL
    },
    ngx_null_command
};


/* nginx http module definition */
static ngx_http_module_t ngx_http_guetzli_filter_module_ctx = {
    NULL,                              /* preconfiguration */
    ngx_http_guetzli_init,             /* postconfiguration */
    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */
    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */
    ngx_http_guetzli_create_loc_conf,  /* create location configuration */
    ngx_http_guetzli_merge_loc_conf    /* merge location configuration */
};


/* nginx module definition */
ngx_module_t ngx_http_guetzli_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_guetzli_filter_module_ctx,      /* module context */
    ngx_http_guetzli_commands,         /* module directives */
    NGX_HTTP_MODULE,                   /* module type */
    NULL,                              /* init master */
    NULL,                              /* init module */
    NULL,                              /* init process */
    NULL,                              /* init thread */ NULL,                              /* exit thread */
    NULL,                              /* exit process */
    NULL,                              /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_http_output_header_filter_pt ngx_http_next_header_filter;

static ngx_int_t
ngx_http_guetzli_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_guetzli_filter;
    return NGX_OK;
}

static void*
ngx_http_guetzli_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_guetzli_loc_conf_t *conf;
  
    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_guetzli_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }
  
    conf->redis_port = NGX_CONF_UNSET_UINT;
  
    return conf;
}

static char*
ngx_http_guetzli_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child)
{
    ngx_http_guetzli_loc_conf_t *prev = parent;
    ngx_http_guetzli_loc_conf_t *conf = child;

    ngx_conf_merge_uint_value(conf->redis_port, prev->redis_port, 6379);

    if (conf->redis_port < 1) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "redis_port must be equal or more than 1");
        return NGX_CONF_ERROR;
    }

    if (conf->redis_port > 65535) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "redis_port must be equal or less than 65535");
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_str_value(conf->redis_host, prev->redis_host, "");
    ngx_conf_merge_str_value(conf->cookie_name, prev->cookie_name, "");

    return NGX_CONF_OK;
}




static ngx_int_t
ngx_http_guetzli_filter(ngx_http_request_t *r)
{
    ngx_int_t rc;
    ngx_http_guetzli_loc_conf_t *conf;
    uuid_t uuid;
    ngx_str_t uuid_str;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_guetzli_filter_module);

    if ( conf->cookie_name.len == 0
        || r->headers_out.status != NGX_HTTP_OK
        || r != r->main
        || r->internal
        || r->error_page
        || r->post_action)
    {
        return ngx_http_next_header_filter(r);
    }

    rc = ngx_http_guetzli_get_cookie(r, conf, &uuid_str);
    if (rc == NGX_OK) {
        rc = redis_lookup_uuid(conf, &uuid_str) == NGX_OK;
        if (rc == NGX_OK) {
            return ngx_http_next_header_filter(r);
        }
    }

    uuid_generate_random(uuid);
    uuid_str.data = ngx_pnalloc(r->pool, 33);
    uuid_str.len = 32;
    ngx_hex_dump(uuid_str.data, (u_char *)uuid, 16);
    uuid_str.data[32] = '\0';

    redis_store_uuid(conf, &uuid_str);
    rc = ngx_http_guetzli_set_cookie(r, conf, &uuid_str);

    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    return ngx_http_next_header_filter(r);
}


static ngx_int_t
redis_lookup_uuid(ngx_http_guetzli_loc_conf_t *conf, ngx_str_t *uuid)
{
    redisContext *context = redisConnect((const char*)conf->redis_host.data, conf->redis_port);
    redisReply *reply = redisCommand(context, "GET %s", uuid->data);
    if (reply->type == REDIS_REPLY_NIL) {
        return NGX_DECLINED;
    } else {
        return NGX_OK;
    }
}

static ngx_int_t
redis_store_uuid(ngx_http_guetzli_loc_conf_t *conf, ngx_str_t *uuid)
{
    redisContext *context = redisConnect((const char*)conf->redis_host.data, conf->redis_port);
    redisReply *reply = redisCommand(context, "SET %s 1", uuid->data);
    if (reply->type == REDIS_REPLY_ERROR) {
        return NGX_DECLINED;
    } else {
        return NGX_OK;
    }
}

static ngx_int_t
ngx_http_guetzli_get_cookie(ngx_http_request_t *r, ngx_http_guetzli_loc_conf_t *conf, ngx_str_t *uuid)
{
    ngx_int_t rc;
 
    rc = ngx_http_parse_multi_header_lines(&r->headers_in.cookies,
        &conf->cookie_name, uuid);

    if (rc == NGX_DECLINED) {
        return NGX_DECLINED;
    }

    if (uuid_verify(uuid) != NGX_OK) {
        return NGX_DECLINED;
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_guetzli_set_cookie(ngx_http_request_t *r, ngx_http_guetzli_loc_conf_t *conf, ngx_str_t *uuid)
{
    u_char *cookie, *p;
    size_t len;
    ngx_table_elt_t *set_cookie;
    ngx_str_t cookie_suffix = ngx_string(";secure;HttpOnly");

    len = conf->cookie_name.len + 1 + 32 + cookie_suffix.len;

    cookie = ngx_pnalloc(r->pool, len);
    if (cookie == NULL) {
        return NGX_ERROR;
    }

    p = ngx_copy(cookie, conf->cookie_name.data, conf->cookie_name.len);
    *p++ = '=';
    p = ngx_copy(p, uuid->data, uuid->len);
    p = ngx_copy(p, cookie_suffix.data, cookie_suffix.len);

    set_cookie = ngx_list_push(&r->headers_out.headers);
    if (set_cookie == NULL) {
        return NGX_ERROR;
    }

    set_cookie->hash = 1;
    ngx_str_set(&set_cookie->key, "Set-Cookie");
    set_cookie->value.len = p - cookie;
    set_cookie->value.data = cookie;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "set cookie: \"%V\"", &set_cookie->value);

    return NGX_OK;
}

static ngx_int_t
uuid_verify(ngx_str_t *in)
{
    ngx_int_t i;
    const u_char *cp;

    for (i = 0, cp = in->data; i < 32; i++, cp++) {
        if (!isxdigit(*cp)) {
            return NGX_ERROR;
        }
    }

    if (*cp != '\0') {
        return NGX_ERROR;
    }

    return NGX_OK;
}

