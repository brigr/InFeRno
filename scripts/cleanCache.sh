#!/bin/sh

mysql -u usr_inferno -p\<passwd\> inferno << _EOF
delete from cache;
quit
_EOF
