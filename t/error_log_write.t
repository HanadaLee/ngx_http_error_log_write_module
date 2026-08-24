#!/usr/bin/perl

# Tests for ngx_http_error_log_write_module.

###############################################################################

use warnings;
use strict;

use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use Test::Nginx;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $t = Test::Nginx->new()->has(qw/http rewrite ngx_condition_module
	ngx_http_error_log_write_module/)->plan(10);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

daemon off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    error_log_write level=notice message=http:$uri:$status;

    server {
        listen       127.0.0.1:8080;
        server_name  localhost;

        error_log %%TESTDIR%%/module.log info;

        condition selected str_eq $arg_log yes;

        when selected {
            error_log_write level=warn message=conditional:$arg_value;
        }

        error_log_write level=error message=server:$uri:$arg_value;

        location = /local {
            error_log_write level=info message=local:$request_method;
            return 200 local;
        }

        location = /inherit {
            return 200 inherit;
        }
    }
}

EOF

$t->run();

###############################################################################

like(http_get('/local?log=yes&value=alpha'), qr/200 OK/,
	'conditional request succeeds');
like(http_get('/inherit?value=beta'), qr/200 OK/,
	'unconditional request succeeds');

my $log = $t->read_file('module.log');

like($log, qr/\[info\].*local:GET/, 'location entry uses configured level');
like($log, qr/\[warn\].*conditional:alpha/,
	'matching condition writes entry with variable');
unlike($log, qr/conditional:beta/,
	'non-matching condition does not write entry');
like($log, qr/\[error\].*server:\/local:alpha/,
	'server entry is inherited by location');
like($log, qr/\[error\].*server:\/inherit:beta/,
	'server entry runs for sibling location');
like($log, qr/\[notice\].*http:\/local:200/,
	'http entry is inherited through server');
like($log, qr/\[notice\].*http:\/inherit:200/,
	'http entry evaluates final response status');

my @local = $log =~ /local:GET/g;
is(scalar @local, 1, 'location entry is written once');

###############################################################################
