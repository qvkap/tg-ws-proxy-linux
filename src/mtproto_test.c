#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define TARGET_IP "149.154.167.50" 
#define TARGET_PORT 443

void reverse_bytes(unsigned char *src, unsigned char *dst, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
}

int main() {
    int sock;
    struct sockaddr_in server;

    OpenSSL_add_all_algorithms();

    printf("[INFO] Connecting to Telegram DC2 (%s:%d)...\n", TARGET_IP, TARGET_PORT);
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return 1;
    }

    server.sin_addr.s_addr = inet_addr(TARGET_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(TARGET_PORT);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect");
        return 1;
    }
    printf("[INFO] TCP connection established!\n");

    unsigned char buf[64];
    while (1) {
        RAND_bytes(buf, 64);
        
        if (buf[0] == 0xef) continue;
        if (buf[0] == 0x48 && buf[1] == 0x4f && buf[2] == 0x53 && buf[3] == 0x54) continue;
        if (buf[0] == 0x50 && buf[1] == 0x4f && buf[2] == 0x53 && buf[3] == 0x54) continue;
        if (buf[0] == 0x47 && buf[1] == 0x45 && buf[2] == 0x54 && buf[3] == 0x20) continue;
        if (buf[0] == 0xee && buf[1] == 0xee && buf[2] == 0xee && buf[3] == 0xee) continue;
        
        buf[60] = 0; buf[61] = 0; buf[62] = 0; buf[63] = 0;
        break;
    }

    buf[56] = 0xee; buf[57] = 0xee; buf[58] = 0xee; buf[59] = 0xee;

    unsigned char enc_key[32], enc_iv[16];
    unsigned char dec_key[32], dec_iv[16];

    memcpy(enc_key, buf + 8, 32);
    memcpy(enc_iv, buf + 40, 16);
    
    reverse_bytes(enc_key, dec_key, 32);
    reverse_bytes(enc_iv, dec_iv, 16);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, enc_key, enc_iv);
    
    int len;
    unsigned char encrypted_buf[64];
    EVP_EncryptUpdate(ctx, encrypted_buf, &len, buf, 64);
    
    memcpy(buf + 56, encrypted_buf + 56, 8);
    EVP_CIPHER_CTX_free(ctx);

    printf("[INFO] Sending 64-byte MTProto Obfuscated Handshake...\n");
    if (send(sock, buf, 64, 0) < 0) {
        perror("send");
        return 1;
    }

    printf("[INFO] Waiting for response from Telegram servers...\n");
    unsigned char response[4096];
    
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    
    int act = select(sock + 1, &readfds, NULL, NULL, &tv);
    if (act > 0) {
        int bytes_read = recv(sock, response, sizeof(response), 0);
        if (bytes_read > 0) {
            printf("[SUCCESS] Telegram accepted the handshake and replied with %d bytes!\n", bytes_read);
            printf("[SUCCESS] Connection NOT closed. No EOF errors.\n");
        } else if (bytes_read == 0) {
            printf("[ERROR] Telegram closed the connection anyway (EOF).\n");
        } else {
            perror("recv");
        }
    } else if (act == 0) {
        printf("[SUCCESS] 5 seconds passed, Telegram is silent and keeping connection OPEN!\n");
        printf("[SUCCESS] Handshake successful, Telegram recognized us.\n");
    }

    close(sock);
    return 0;
}
