#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <openssl/ssl.h>
#include <openssl/err.h>


SSL *ws_connect(const char *host, int port, const char *path);


int ws_send_frame(SSL *ssl, const unsigned char *data, size_t len);


int ws_recv_frame(SSL *ssl, unsigned char *out_buf, size_t max_len);


void ws_close(SSL *ssl);

#endif 
