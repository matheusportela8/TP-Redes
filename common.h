#pragma once

#include <stdlib.h>

#include <arpa/inet.h>

void logexit(const char *msg);

int addrparse(const char *rcvd_ip, const char *rcvd_port, struct sockaddr_storage *storage);

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);

int server_sockaddr_init(const char *rcvd_ip_family, const char *rcvd_port,
                         struct sockaddr_storage *storage);

enum MSG_TYPE {REQ_ADDPEER = 1, REQ_RMPEER, REQ_ADDC2P, REQ_REMCFP, REQ_ADD, REQ_REM, RES_ADD,RES_LIST,REQ_INF, RES_INF, ERROR, OK};
unsigned parse_msg_type(const char *msg_type_in);