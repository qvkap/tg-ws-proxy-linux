#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <openssl/ssl.h>
#include <openssl/err.h>

// Подключение к WebSocket-серверу (HTTPS -> WSS)
SSL *ws_connect(const char *host, int port, const char *path);

// Отправка бинарного WebSocket фрейма
int ws_send_frame(SSL *ssl, const unsigned char *data, size_t len);

// Чтение бинарного WebSocket фрейма (возвращает количество прочитанных байт)
int ws_recv_frame(SSL *ssl, unsigned char *out_buf, size_t max_len);

// Закрытие соединения
void ws_close(SSL *ssl);

#endif // WS_CLIENT_H
