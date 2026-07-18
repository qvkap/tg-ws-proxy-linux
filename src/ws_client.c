#include "ws_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

// Вспомогательная функция кодирования в Base64
static void base64_encode(const unsigned char *input, int length, char *output) {
    EVP_EncodeBlock((unsigned char *)output, input, length);
}

SSL *ws_connect(const char *host, int port, const char *path) {
    // 1. Инициализация OpenSSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) return NULL;

    // 2. Создание TCP сокета
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct hostent *server = gethostbyname(host);
    if (!server) return NULL;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return NULL;
    }

    // 3. Установка TLS соединения
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sock);
    SSL_set_tlsext_host_name(ssl, host); // SNI

    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl);
        close(sock);
        return NULL;
    }

    // 4. Генерация ключа Sec-WebSocket-Key
    unsigned char random_key[16];
    char b64_key[25];
    RAND_bytes(random_key, 16);
    base64_encode(random_key, 16, b64_key);

    // 5. Отправка HTTP Upgrade запроса
    char request[1024];
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Key: %s\r\n"
             "Sec-WebSocket-Version: 13\r\n"
             "\r\n", path, host, b64_key);

    SSL_write(ssl, request, strlen(request));

    // 6. Чтение ответа сервера (очень упрощенно)
    char response[2048];
    int bytes = SSL_read(ssl, response, sizeof(response) - 1);
    if (bytes > 0) {
        response[bytes] = '\0';
        if (strstr(response, "101 Switching Protocols") != NULL) {
            printf("[WS] Successful WSS connection to %s\n", host);
            return ssl;
        }
    }

    printf("[WS] WSS Upgrade failed\n");
    SSL_free(ssl);
    close(sock);
    return NULL;
}

int ws_send_frame(SSL *ssl, const unsigned char *data, size_t len) {
    unsigned char frame[8192];
    int header_len = 0;
    
    frame[0] = 0x82; // FIN + Binary frame
    
    if (len < 126) {
        frame[1] = len | 0x80; // С маской
        header_len = 2;
    } else if (len <= 65535) {
        frame[1] = 126 | 0x80;
        frame[2] = (len >> 8) & 0xFF;
        frame[3] = len & 0xFF;
        header_len = 4;
    } else {
        return -1; // Слишком большой фрейм для упрощенной версии
    }

    // Генерация маски (клиент обязан маскировать данные)
    unsigned char mask[4];
    RAND_bytes(mask, 4);
    memcpy(frame + header_len, mask, 4);
    header_len += 4;

    // Маскируем данные
    for (size_t i = 0; i < len; i++) {
        frame[header_len + i] = data[i] ^ mask[i % 4];
    }

    return SSL_write(ssl, frame, header_len + len);
}

int ws_recv_frame(SSL *ssl, unsigned char *out_buf, size_t max_len) {
    unsigned char header[2];
    int bytes = SSL_read(ssl, header, 2);
    if (bytes <= 0) return bytes;

    int payload_len = header[1] & 0x7F;
    int is_masked = (header[1] & 0x80) != 0;

    if (payload_len == 126) {
        unsigned char ext_len[2];
        SSL_read(ssl, ext_len, 2);
        payload_len = (ext_len[0] << 8) | ext_len[1];
    }

    if (payload_len > (int)max_len) return -1; // Буфер маловат

    unsigned char mask[4] = {0};
    if (is_masked) {
        SSL_read(ssl, mask, 4);
    }

    int total_read = 0;
    while (total_read < payload_len) {
        int r = SSL_read(ssl, out_buf + total_read, payload_len - total_read);
        if (r <= 0) return r;
        total_read += r;
    }

    if (is_masked) {
        for (int i = 0; i < payload_len; i++) {
            out_buf[i] ^= mask[i % 4];
        }
    }

    return payload_len;
}

void ws_close(SSL *ssl) {
    if (ssl) {
        int sock = SSL_get_fd(ssl);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(sock);
    }
}
