# Name

`ngx_http_error_log_write_module` allows writing error log entries based on conditional expressions in nginx configuration files..

# Table of Content

- [Name](#name)
- [Table of Content](#table-of-content)
- [Status](#status)
- [Synopsis](#synopsis)
- [Installation](#installation)
- [Conditional syntax](#conditional-syntax)
- [Directives](#directives)
  - [error\_log\_write](#error_log_write)
- [Author](#author)
- [License](#license)

# Status

This Nginx module is currently considered experimental. Issues and PRs are welcome if you encounter any problems.

# Synopsis

```nginx
error_log_write level=info "message=main test log";

server {
    listen 127.0.0.1:80;
    server_name localhost;

    # With ngx_condition_module
    condition test_enabled is_not_empty $arg_test;
    when test_enabled {
        error_log_write "message=server test log";
    }

    location / {
        condition missing_authorization is_empty $http_authorization;
        when missing_authorization {
            error_log_write level=warn "message=auth required";
        }

        # Without ngx_condition_module, use this instead:
        # error_log_write level=warn "message=auth required" if!=$http_authorization;
        auth_basic "auth required";
        auth_basic_user_file conf/htpasswd;
        proxy_pass http://example.upstream.com;
    }
}
```

# Installation

To use theses modules, configure your nginx branch with `--add-module=/path/to/ngx_http_error_log_write_module`.

To enable named conditions, build `ngx_condition_module` and this module statically in the same nginx configuration.

# Conditional syntax

Conditional syntax is selected at compile time. When `ngx_condition_module` is enabled, place `error_log_write` inside an `http`, `server`, or `location` `when` block. The legacy `if=` and `if!=` parameters are rejected. Without `ngx_condition_module`, `when` is unavailable and the legacy parameters remain supported. `if=` matches a non-empty value other than `"0"`; `if!=` matches an empty value or `"0"`.

# Directives

## error_log_write

**Syntax:** *error_log_write [level=log_level] message=text;*

**Default:** *-*

**Context:** *http, server, location, http when, server when, location when*

Writing a new error log. All error log entries are inherited unconditionally from the previous configuration level.

# Author

Hanada im@hanada.info

# License

This Nginx module is licensed under [BSD 2-Clause License](LICENSE).
