/*
 * ngx_http_guetzli_module
 *
 * (c) 2014 Antoine Neuenschwander <antoine@schoggi.org>
 */

#include <nginx.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <string.h>
#include "hiredis/hiredis.h"


/* module configuration structure */
typedef struct {
    ngx_str_t redis_host;
    ngx_int_t redis_port;
    ngx_str_t cookie_name;
} ngx_http_guetzli_loc_conf_t;


/* module functions */
static ngx_int_t ngx_http_guetzli_init(ngx_conf_t *cf);
static void* ngx_http_guetzli_create_loc_conf(ngx_conf_t *cf);
static char* ngx_http_guetzli_merge_loc_conf(ngx_conf_t* cf, void* parent, void* child);
static ngx_int_t ngx_http_guetzli_filter(ngx_http_request_t *r);
static void append_debug_header(ngx_http_request_t *r, ngx_str_t *hdr_value);
static ngx_int_t lookup_session_cookie(ngx_http_guetzli_loc_conf_t *conf, ngx_str_t *guetzli_sess, ngx_str_t *guetzli_values);


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
static ngx_http_module_t ngx_http_guetzli_module_ctx = {
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
ngx_module_t ngx_http_guetzli_module = {
    NGX_MODULE_V1,
    &ngx_http_guetzli_module_ctx,      /* module context */
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
    return NGX_CONF_OK;
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
    ngx_http_guetzli_loc_conf_t *conf = ngx_http_get_module_loc_conf(r, ngx_http_guetzli_module);

    if ( conf->cookie_name.len == 0
        || r->headers_out.status != NGX_HTTP_OK
        || r != r->main
        || r->internal
        || r->error_page
        || r->post_action)
    {
        return ngx_http_next_header_filter(r);
    }

    ngx_int_t lookup;
    ngx_str_t sess_cookie;
    lookup = ngx_http_parse_multi_header_lines(&r->headers_in.cookies, &conf->cookie_name, &sess_cookie);

  if (lookup != NGX_DECLINED) {
      ngx_str_t values;
      ngx_int_t lookup_result = lookup_session_cookie(conf, &sess_cookie, &values);
      
      if (lookup_result != NGX_DECLINED) {
          append_debug_header(r, &values);
      }
  }

  return ngx_http_next_header_filter(r);
}



static void
append_debug_header(ngx_http_request_t *r, ngx_str_t *hdr_value)
{
    ngx_table_elt_t *h;
    h = ngx_list_push(&r->headers_out.headers);
    h->hash = 1;
    ngx_str_set(&h->key, "X-Guetzli-Debug");
    h->value = *hdr_value;
}


static ngx_int_t
lookup_session_cookie(ngx_http_guetzli_loc_conf_t *conf, ngx_str_t *sess_cookie, ngx_str_t *values)
{
    redisContext *context = redisConnect((const char*)conf->redis_host.data, conf->redis_port);
    redisReply *reply = redisCommand(context, "GET %s", sess_cookie->data);
    if (reply->type == REDIS_REPLY_NIL) {
        return NGX_DECLINED;
    } else {
        values->len = strlen(reply->str);
        values->data = reply->str;
        return NGX_OK;
    }
}

