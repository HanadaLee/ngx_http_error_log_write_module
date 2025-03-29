# Name

`ngx_http_error_log_write_module` allows conditional writing of custom error log entries in nginx configuration.

# Table of Content

- [Name](#name)
- [Table of Content](#table-of-content)
- [Status](#status)
- [Synopsis](#synopsis)
- [Installation](#installation)
- [Directives](#directives)
  - [log\_var\_set](#error_log_write)
- [Author](#author)
- [License](#license)

# Status

This Nginx module is currently considered experimental. Issues and PRs are welcome if you encounter any problems.

# Synopsis

```nginx
error_log_write level=info message="main test log";

server {
    listen 127.0.0.1:80;
    server_name localhost;

    error_log_write  message="server test log" if=$arg_test; 

    location / {
        error_log_write level=warn message="auth required" if!=$http_authorization;
        auth_baisc "auth required";
        auth_basic_user_file conf/htpasswd;
        proxy_pass http://example.upstream.com;
    }
}
```

# Installation

To use theses modules, configure your nginx branch with `--add-module=/path/to/ngx_http_error_log_write_module`.

# Directives

## error_log_write

**Syntax:** *error_log_write [level=log_level] message=text [if=condition];*

**Default:** *-*

**Context:** *http, server, location*

Writing a new error log. All error log entries are inherited unconditionally from the previous configuration level.

# Author

Hanada im@hanada.info

# License

This Nginx module is licensed under [BSD 2-Clause License](LICENSE).
