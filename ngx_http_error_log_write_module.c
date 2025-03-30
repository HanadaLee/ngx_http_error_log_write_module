
/*
 * Copyright (C) Hanada
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_uint_t                  level;
    ngx_http_complex_value_t   *message;
    ngx_http_complex_value_t   *filter;
    ngx_int_t                   negative;
} ngx_http_error_log_write_entry_t;


typedef struct {
    ngx_array_t                 *log_entries;
} ngx_http_error_log_write_loc_conf_t;


static ngx_int_t ngx_http_error_log_write_init(ngx_conf_t *cf);
static void *ngx_http_error_log_write_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_error_log_write_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
static char *ngx_http_error_log_write(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static ngx_int_t ngx_http_error_log_write_handler(ngx_http_request_t *r);


static ngx_command_t ngx_http_error_log_write_commands[] = {

    { ngx_string("error_log_write"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE123,
      ngx_http_error_log_write,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t ngx_http_error_log_write_module_ctx = {
    NULL,                                           /* preconfiguration */
    ngx_http_error_log_write_init,                  /* postconfiguration */
    NULL,                                           /* create main configuration */
    NULL,                                           /* init main configuration */
    NULL,                                           /* create server configuration */
    NULL,                                           /* merge server configuration */
    ngx_http_error_log_write_create_loc_conf,       /* create location configuration */
    ngx_http_error_log_write_merge_loc_conf         /* merge location configuration */
};


ngx_module_t ngx_http_error_log_write_module = {
    NGX_MODULE_V1,
    &ngx_http_error_log_write_module_ctx,           /* module context */
    ngx_http_error_log_write_commands,              /* module directives */
    NGX_HTTP_MODULE,                                /* module type */
    NULL,                                           /* init master */
    NULL,                                           /* init module */
    NULL,                                           /* init process */
    NULL,                                           /* init thread */
    NULL,                                           /* exit thread */
    NULL,                                           /* exit process */
    NULL,                                           /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_error_log_write_handler(ngx_http_request_t *r)
{
    ngx_http_error_log_write_loc_conf_t  *elwcf;
    ngx_http_error_log_write_entry_t     *entries;
    ngx_str_t                             message;
    ngx_uint_t                            i;
    ngx_str_t                             val;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "error_log_write handler");

    elwcf = ngx_http_get_module_loc_conf(r, ngx_http_error_log_write_module);

    if (elwcf->log_entries == NULL || elwcf->log_entries->nelts == 0) {
        return NGX_DECLINED;
    }

    entries = elwcf->log_entries->elts;

    for (i = 0; i < elwcf->log_entries->nelts; i++) {

        if (entries[i].filter) {
            if (ngx_http_complex_value(r, entries[i].filter, &val)
                    != NGX_OK)
            {
                return NGX_ERROR;
            }

            if (val.len == 0 || (val.len == 1 && val.data[0] == '0')) {
                if (!entries[i].negative) {
                    continue;
                }
            } else {
                if (entries[i].negative) {
                    continue;
                }
            }
        }

        if (ngx_http_complex_value(r, entries[i].message, &message)
                != NGX_OK)
        {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                          "error_log_write: failed to evaluate message");
            continue;
        }

        if (entries[i].level == NGX_LOG_DEBUG) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                      "error_log_write: %V", &message);
        }

        ngx_log_error(entries[i].level, r->connection->log, 0,
                      "error_log_write: %V", &message);
    }

    return NGX_DECLINED;
}


static char *
ngx_http_error_log_write(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_error_log_write_loc_conf_t  *elwcf = conf;

    ngx_str_t                            *value;
    ngx_http_error_log_write_entry_t     *entry;
    ngx_uint_t                            n;
    ngx_str_t                             s;
    ngx_http_compile_complex_value_t      ccv;

    if (elwcf->log_entries == NULL) {
        elwcf->log_entries = ngx_array_create(cf->pool, 1,
                                sizeof(ngx_http_error_log_write_entry_t));
        if (elwcf->log_entries == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    entry = ngx_array_push(elwcf->log_entries);
    if (entry == NULL) {
        return NGX_CONF_ERROR;
    }

    entry->level = NGX_LOG_ERR;
    entry->filter = NULL;
    entry->negative = 0;

    value = cf->args->elts;

    for (n = 1; n < cf->args->nelts; n++) {

        if (ngx_strncmp(value[n].data, "level=", 6) == 0) {
            s.len = value[n].len - 6;
            s.data = value[n].data + 6;

            if (s.len == 6 && ngx_strncmp(s.data, "stderr", 6) == 0) {
                entry->level = NGX_LOG_STDERR;
                continue;
            }

            if (s.len == 5 && ngx_strncmp(s.data, "emerg", 5) == 0) {
                entry->level = NGX_LOG_EMERG;
                continue;
            }

            if (s.len == 5 && ngx_strncmp(s.data, "alert", 5) == 0) {
                entry->level = NGX_LOG_ALERT;
                continue;
            }

            if (s.len == 4 && ngx_strncmp(s.data, "crit", 4) == 0) {
                entry->level = NGX_LOG_CRIT;
                continue;
            }

            if ((s.len == 3 && ngx_strncmp(s.data, "err", 3) == 0)
                || (s.len == 5 && ngx_strncmp(s.data, "error", 5) == 0)) {
                entry->level = NGX_LOG_ERR;
                continue;
            }

            if (s.len == 4 && ngx_strncmp(s.data, "warn", 4) == 0) {
                entry->level = NGX_LOG_WARN;
                continue;
            }

            if (s.len == 5 && ngx_strncmp(s.data, "notice", 5) == 0) {
                entry->level = NGX_LOG_NOTICE;
                continue;
            }

            if (s.len == 4 && ngx_strncmp(s.data, "info", 4) == 0) {
                entry->level = NGX_LOG_INFO;
                continue;
            }

            if (s.len == 5 && ngx_strncmp(s.data, "debug", 5) == 0) {
#if (NGX_DEBUG)
                entry->level = NGX_LOG_DEBUG;
                continue;
#else
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                "nginx was built without debug support");
                return NGX_CONF_ERROR;
#endif
            }

            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                    "invalid log level \"%V\"", &s);
            return NGX_CONF_ERROR;
        }

        if (ngx_strncmp(value[n].data, "message=", 8) == 0) {
            s.data = value[n].data + 8;
            s.len = value[n].len - 8;

            ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
            ccv.cf = cf;
            ccv.value = &s;
            ccv.complex_value = ngx_palloc(cf->pool,
                                        sizeof(ngx_http_complex_value_t));

            if (ccv.complex_value == NULL) {
                return NGX_CONF_ERROR;
            }

            if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            entry->message = ccv.complex_value;

            continue;
        }

        if (ngx_strncmp(value[n].data, "if=", 3) == 0) {
            s.len = value[n].len - 3;
            s.data = value[n].data + 3;

            ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
            ccv.cf = cf;
            ccv.value = &s;
            ccv.complex_value = ngx_palloc(cf->pool,
                                        sizeof(ngx_http_complex_value_t));

            if (ccv.complex_value == NULL) {
                return NGX_CONF_ERROR;
            }

            if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }
            
            entry->filter = ccv.complex_value;
            entry->negative = 0;

            continue;
        }

        if (ngx_strncmp(value[n].data, "if!=", 4) == 0) {
            s.len = value[n].len - 4;
            s.data = value[n].data + 4;

            ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
            ccv.cf = cf;
            ccv.value = &s;
            ccv.complex_value = ngx_palloc(cf->pool,
                                        sizeof(ngx_http_complex_value_t));

            if (ccv.complex_value == NULL) {
                return NGX_CONF_ERROR;
            }

            if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            entry->filter = ccv.complex_value;
            entry->negative = 1;

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[n]);
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static void *
ngx_http_error_log_write_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_error_log_write_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_error_log_write_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->log_entries = NULL;

    return conf;
}


static char *
ngx_http_error_log_write_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child)
{
    ngx_http_error_log_write_loc_conf_t  *prev = parent;
    ngx_http_error_log_write_loc_conf_t  *conf = child;

    ngx_uint_t                            i;
    ngx_http_error_log_write_entry_t     *entry;
    ngx_http_error_log_write_entry_t     *prev_entries;

    if (conf->log_entries == NULL || conf->log_entries->nelts == 0) {
        conf->log_entries = prev->log_entries;
        return NGX_CONF_OK;
    }

    if (prev->log_entries == NULL || prev->log_entries->nelts == 0) {
        return NGX_CONF_OK;
    }

    prev_entries = prev->log_entries->elts;

    for (i = 0; i < prev->log_entries->nelts; i++) {
        entry = ngx_array_push(conf->log_entries);
        if (entry == NULL) {
            return NGX_CONF_ERROR;
        }

        *entry = prev_entries[i];
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_error_log_write_init(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t *cmcf;
    ngx_http_handler_pt       *h;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_error_log_write_handler;

    return NGX_OK;
}