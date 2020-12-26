// Copyright (C) 2013 - Will Glozer.  All rights reserved.

#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "net.h"
#include <errno.h>
#include <string.h>
#include "islog.h"



status sock_connect(connection *c, char *host) {
    return OK;
}

status sock_close(connection *c) {
    return OK;
}

status sock_read(connection *c, size_t *n) {
    ssize_t r = read(c->fd, c->buf, sizeof(c->buf));
    *n = (size_t) r;
    if( r < 0){
        islog_error("sock_read error : %s", strerror(errno));
    }
    return r >= 0 ? OK : ERROR;
}

status sock_write(connection *c, char *buf, size_t len, size_t *n) {
    ssize_t r;
    if ((r = write(c->fd, buf, len)) == -1) {
        switch (errno) {
            case EAGAIN: return RETRY;
            default:
                islog_error("sock_write error : %s", strerror(errno));
                return ERROR;
        }
    }
    *n = (size_t) r;
    return OK;
}

size_t sock_readable(connection *c) {
    int n, rc;
    rc = ioctl(c->fd, FIONREAD, &n);
    return rc == -1 ? 0 : n;
}
