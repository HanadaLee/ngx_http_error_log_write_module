
/*
 * Copyright (C) Hanada
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_uint_t                  level;
    ngx_http_complex_value_t    message;
} ngx_http_error_log_write_entry_t;


typedef struct {
    ngx_array_t                *phases[NGX_HTTP_LOG_PHASE + 1];
} ngx_http_error_log_write_loc_conf_t;


static ngx_int_t ngx_http_error_log_write_init(ngx_conf_t *cf);
static void *ngx_http_error_log_write_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_error_log_write_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);
static char *ngx_http_error_log_write(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);
static ngx_int_t ngx_http_error_log_write_phase_handler(ngx_http_request_t *r);


static ngx_conf_enum_t ngx_http_error_log_write_phases[] = {
    { ngx_string("post_read"), NGX_HTTP_POST_READ_PHASE },
    { ngx_string("server_rewrite"), NGX_HTTP_SERVER_REWRITE_PHASE },
    { ngx_string("rewrite"), NGX_HTTP_REWRITE_PHASE },
    { ngx_string("pre_access"), NGX_HTTP_PRE_ACCESS_PHASE },
    { ngx_string("access"), NGX_HTTP_ACCESS_PHASE },
    { ngx_string("pre_content"), NGX_HTTP_PRE_CONTENT_PHASE },
    { ngx_string("content"), NGX_HTTP_CONTENT_PHASE },
    { ngx_string("log"), NGX_HTTP_LOG_PHASE },
    { ngx_null_string, 0 }
};


static ngx_conf_enum_t ngx_http_error_log_write_levels[] = {
    { ngx_string("emerg"), NGX_LOG_EMERG },
    { ngx_string("alert"), NGX_LOG_ALERT },
    { ngx_string("crit"), NGX_LOG_CRIT },
    { ngx_string("error"), NGX_LOG_ERR },
    { ngx_string("warn"), NGX_LOG_WARN },
    { ngx_string("notice"), NGX_LOG_NOTICE },
    { ngx_string("info"), NGX_LOG_INFO },
    { ngx_string("debug"), NGX_LOG_DEBUG },
    { ngx_null_string, 0 }
};


static ngx_command_t ngx_http_error_log_write_commands[] = {

    { ngx_string("error_log_write"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1234,
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
    ngx_str_t                          val;
    ngx_http_variable_t               *v;
    ngx_http_variable_value_t         *vv;
    ngx_http_error_log_write_loc_conf_t   *llcf;
    ngx_http_error_log_write_variable_t   *lv, *last;
    ngx_http_core_main_conf_t         *cmcf;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "log var set handler");

    llcf = ngx_http_get_module_loc_conf(r, ngx_http_error_log_write_module);

    if (llcf->vars == NULL) {
        return NGX_OK;
    }

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);
    v = cmcf->variables.elts;

    lv = llcf->vars->elts;
    last = lv + llcf->vars->nelts;

    while (lv < last) {

        if (lv->filter) {
            if (ngx_http_complex_value(r, lv->filter, &val)
                    != NGX_OK) {
                return NGX_ERROR;
            }

            if (val.len == 0 || (val.len == 1 && val.data[0] == '0')) {
                if (!lv->negative) {
                    lv++;
                    continue;
                }
            } else {
                if (lv->negative) {
                    lv++;
                    continue;
                }
            }
        }

        /*
         * explicitly set new value to make sure it will be available after
         * internal redirects
         */

        vv = &r->variables[lv->index];

        if (ngx_http_complex_value(r, &lv->value, &val) != NGX_OK) {
            return NGX_ERROR;
        }

        vv->valid = 1;
        vv->not_found = 0;
        vv->data = val.data;
        vv->len = val.len;

        if (lv->set_handler) {
            /*
             * set_handler only available in cmcf->variables_keys, so we store
             * it explicitly
             */

            lv->set_handler(r, vv, v[lv->index].data);
        }

        lv++;
    }

    return NGX_OK;
}


static char *
ngx_http_error_log_write(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_error_log_write_loc_conf_t *elwcf = conf;

    
    ngx_uint_t                          phase;
    ngx_uint_t                          level;
    ngx_http_complex_value_t            cv;
    ngx_str_t                          *value, s;
    ngx_uint_t                          n;
    ngx_http_error_log_write_entry_t   *entry;
    ngx_array_t                         entries;

    value = cf->args->elts;
    phase = NGX_HTTP_LOG_PHASE;
    level = NGX_LOG_ERR;

    ngx_memzero(&cv, sizeof(ngx_http_complex_value_t));

    for (n = 1; n < cf->args->nelts; n++) {

        if (ngx_strncmp(value[n].data, "phase=", 6) == 0) {
            s.len = value[n].len - 6;
            s.data = value[n].data + 6;

            if (ngx_conf_parse_enum(cf, ngx_http_error_log_write_phases,
                    &s, &phase) != NGX_CONF_OK)
            {
                return NGX_CONF_ERROR;
            }

            continue;
        }

        if (ngx_strncmp(value[n].data, "level=", 6) == 0) {
            s.len = value[n].len - 6;
            s.data = value[n].data + 6;

            if (ngx_conf_parse_enum(cf, ngx_http_error_log_write_levels,
                    &s, &level) != NGX_CONF_OK)
            {
                return NGX_CONF_ERROR;
            }
            
            continue;
        }

        if (ngx_strncmp(value[n].data, "message=", 8) == 0) {
            s.data = value[n].data + 8;
            s.len = value[n].len - 8;

            if (ngx_http_compile_complex_value(&cv, &message) != NGX_OK) {
                return NGX_CONF_ERROR;
            }

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "invalid parameter \"%V\"", &value[n]);
        return NGX_CONF_ERROR;
    }

    if (cv.value.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "error log write: "
            "message parameter is required");
        return NGX_CONF_ERROR;
    }

    *entries = &elwcf->phases[phase];

    if (*entries == NULL) {
        *entries = ngx_array_create(cf->pool, 1,
            sizeof(ngx_http_error_log_write_entry_t));
        if (*entries == NULL) {
            return NGX_CONF_ERROR;
        }
    }

    entry = ngx_array_push(*entries);
    if (entry == NULL) {
        return NGX_CONF_ERROR;
    }

    entry->level = level;
    entry->message = cv;

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

    ngx_memzero(conf->phases, sizeof(conf->phases));
    return conf;
}


static char *
ngx_http_error_log_write_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child)
{
    ngx_http_error_log_write_loc_conf_t  *prev = parent;
    ngx_http_error_log_write_loc_conf_t  *conf = child;

    ngx_uint_t                            i, j;
    ngx_http_error_log_write_entry_t     *entry;

    for (i = 0; i < NGX_HTTP_LOG_PHASE + 1; i++) {

        if (conf->phases[i] == NULL) {
            conf->phases[i] = prev->phases[i];

        } else {
            for (j = 0; j < prev->phases[i]->nelts; j++) {
                entry = ngx_array_push(conf->phases[i]);
                if (entry == NULL) {
                    return NGX_CONF_ERROR;
                }

                *entry = prev->phases[i]->elts[j];
            }
        }
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_error_log_write_init(ngx_conf_t *cf)
{
    ngx_http_core_main_conf_t *cmcf;
    ngx_uint_t i;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    ngx_uint_t target_phases[] = {
        NGX_HTTP_POST_READ_PHASE,
        NGX_HTTP_SERVER_REWRITE_PHASE,
        NGX_HTTP_REWRITE_PHASE,
        NGX_HTTP_PRE_ACCESS_PHASE,
        NGX_HTTP_ACCESS_PHASE,
        NGX_HTTP_PRE_CONTENT_PHASE,
        NGX_HTTP_CONTENT_PHASE,
        NGX_HTTP_LOG_PHASE
    };

    for (i = 0; i < sizeof(target_phases)/sizeof(target_phases[0]); i++) {
        ngx_http_phase_handler_t *ph;

        ph = ngx_array_push(&cmcf->phases[target_phases[i]].handlers);
        if (ph == NULL) {
            return NGX_ERROR;
        }

        ph->checker = ngx_http_core_generic_phase;
        ph->handler = ngx_http_error_log_write_phase_handler;
        ph->next = NGX_HTTP_PHASE_LAST;
    }

    return NGX_OK;
}