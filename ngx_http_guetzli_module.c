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
} guetzli_main_conf_t;


/* module functions */
static void* ngx_http_guetzli_create_main_conf(ngx_conf_t *cf);
static ngx_int_t ngx_http_guetzli_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_guetzli_handler(ngx_http_request_t *r);
static void append_debug_header(ngx_http_request_t *r, ngx_str_t *hdr_value);
static ngx_int_t lookup_session_cookie(guetzli_main_conf_t *conf, ngx_str_t *guetzli_sess, ngx_str_t *guetzli_values);


/* definition of configuration directives */
static ngx_command_t ngx_http_guetzli_commands[] = {
  {
    ngx_string("guetzli_redis_host"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(guetzli_main_conf_t, redis_host),
    NULL
  },
  {
    ngx_string("guetzli_redis_port"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_num_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(guetzli_main_conf_t, redis_port),
    NULL
  },
  {
    ngx_string("guetzli_cookie_name"),
    NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
    ngx_conf_set_str_slot,
    NGX_HTTP_MAIN_CONF_OFFSET,
    offsetof(guetzli_main_conf_t, cookie_name),
    NULL
  },
};


/* nginx http module definition */
static ngx_http_module_t ngx_http_guetzli_module_ctx = {
  NULL,                              /* preconfiguration */
  ngx_http_guetzli_init,             /* postconfiguration */
  ngx_http_guetzli_create_main_conf, /* create main configuration */
  NULL,                              /* init main configuration */
  NULL,                              /* create server configuration */
  NULL,                              /* merge server configuration */
  NULL,                              /* create location configuration */
  NULL                               /* merge location configuration */
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
  NULL,                              /* init thread */
  NULL,                              /* exit thread */
  NULL,                              /* exit process */
  NULL,                              /* exit master */
  NGX_MODULE_V1_PADDING
};


static void*
ngx_http_guetzli_create_main_conf(ngx_conf_t *cf)
{
  guetzli_main_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(guetzli_main_conf_t));
  if (conf == NULL) {
    return NULL;
  }

  conf->redis_port = NGX_CONF_UNSET_UINT;

  return conf;
}




static ngx_int_t
ngx_http_guetzli_handler(ngx_http_request_t *r)
{
  if (r->main->internal) {
    return NGX_DECLINED;
  }

  r->main->internal = 1;
 
  guetzli_main_conf_t *conf = ngx_http_get_module_main_conf(r, ngx_http_guetzli_module);

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

  return NGX_DECLINED;
}



static ngx_int_t
ngx_http_guetzli_init(ngx_conf_t *cf)
{
  ngx_http_handler_pt *h;
  ngx_http_core_main_conf_t *cmcf;

  cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

  h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
  if (h == NULL) {
    return NGX_ERROR;
  }

  *h = ngx_http_guetzli_handler;

  return NGX_OK;
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
lookup_session_cookie(guetzli_main_conf_t *conf, ngx_str_t *sess_cookie, ngx_str_t *values)
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

