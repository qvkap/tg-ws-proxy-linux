#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/select.h>
#include <errno.h>
#include <openssl/rand.h>
#include <sys/stat.h>
#include <syslog.h>
#include <stdarg.h>
#include <time.h>
#include "ws_client.h"

#define DEFAULT_PORT 8080
#define BUFFER_SIZE 8192
#define TARGET_PATH "/apiws"
#define CONFIG_FILE "config.json"

int run_as_daemon = 0;

// Массив зашифрованных доменов Cloudflare из оригинального скрипта
const char* CFPROXY_ENC[] = {
    "virkgj.com", "vmmzovy.com", "mkuosckvso.com", "zaewayzmplad.com",
    "twdmbzcm.com", "awzwsldi.com", "clngqrflngqin.com", "tjacxbqtj.com",
    "bxaxtxmrw.com", "dmohrsgmohcrwb.com", "vwbmtmoi.com", "khgrre.com",
    "ulihssf.com", "tmhqsdqmfpmk.com", "xwuwoqbm.com", "orgcnunpj.com",
    "zhkuldz.com", "zypoljnslxa.com", "efabnxaowuzs.com", "zaftuzsftqdq.com"
};
#define CFPROXY_COUNT (sizeof(CFPROXY_ENC)/sizeof(CFPROXY_ENC[0]))

// Кастомный логгер (Syslog в демоне, stdout в консоли)
void proxy_log(int level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (run_as_daemon) {
        vsyslog(level == 0 ? LOG_INFO : LOG_ERR, fmt, args);
    } else {
        vprintf(fmt, args);
    }
    va_end(args);
}

// Расшифровка домена по алгоритму _dd() из Python
void decode_domain(const char *s, char *out) {
    int len = strlen(s);
    if (len < 4 || strcmp(s + len - 4, ".com") != 0) {
        strcpy(out, s);
        return;
    }
    
    int p_len = len - 4;
    int n = 0;
    for (int i = 0; i < p_len; i++) {
        if ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z')) n++;
    }
    
    for (int i = 0; i < p_len; i++) {
        char c = s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            char base = (c >= 'a') ? 'a' : 'A';
            int diff = (c - base - n);
            out[i] = ((diff % 26) + 26) % 26 + base;
        } else {
            out[i] = c;
        }
    }
    strcpy(out + p_len, ".co.uk");
}

// Функция автозапуска
void install_autostart(int argc, char *argv[]) {
    if (geteuid() != 0) {
        printf("[INFO] To configure autostart, the program needs root privileges.\n");
        printf("[INFO] Asking for password via 'doas' or 'sudo'...\n");
        char cmd[1024] = {0};
        if (system("command -v doas > /dev/null 2>&1") == 0) strcat(cmd, "doas ");
        else strcat(cmd, "sudo ");
        
        char exe_path[512];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
        if (len != -1) { exe_path[len] = '\0'; strcat(cmd, exe_path); }
        else strcat(cmd, argv[0]);

        for (int i = 1; i < argc; i++) { strcat(cmd, " "); strcat(cmd, argv[i]); }
        exit(system(cmd) == 0 ? 0 : 1);
    }

    char exe_path[512];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path)-1);
    if (len == -1) strcpy(exe_path, "/opt/tg-ws-proxy-linux/tg-ws-proxy"); else exe_path[len] = '\0';

    char *user = getenv("SUDO_USER");
    if (!user) user = getenv("DOAS_USER");
    if (!user) user = "root";

    if (access("/run/systemd/system", F_OK) == 0) {
        FILE *f = fopen("/etc/systemd/system/tg-ws-proxy.service", "w");
        if (f) {
            fprintf(f, "[Unit]\nDescription=Telegram WebSocket Proxy\nAfter=network.target\n\n");
            fprintf(f, "[Service]\nType=forking\nUser=%s\n", user);
            fprintf(f, "ExecStart=%s 8080 --daemon\nRestart=always\nRestartSec=3\n\n", exe_path);
            fprintf(f, "[Install]\nWantedBy=multi-user.target\n");
            fclose(f);
            system("systemctl daemon-reload");
            system("systemctl enable tg-ws-proxy");
            system("systemctl start tg-ws-proxy");
        }
    } else if (access("/run/openrc", F_OK) == 0) {
        FILE *f = fopen("/etc/init.d/tg-ws-proxy", "w");
        if (f) {
            fprintf(f, "#!/sbin/openrc-run\n\nname=\"tg-ws-proxy\"\n");
            fprintf(f, "command=\"%s\"\ncommand_args=\"8080 --daemon\"\n", exe_path);
            fprintf(f, "pidfile=\"/run/${RC_SVCNAME}.pid\"\n");
            fprintf(f, "command_user=\"%s\"\n\ndepend() {\n    need net\n}\n", user);
            fclose(f);
            chmod("/etc/init.d/tg-ws-proxy", 0755);
            system("rc-update add tg-ws-proxy default");
            system("rc-service tg-ws-proxy start");
        }
    } else {
        system("mkdir -p /etc/dinit.d");
        FILE *f = fopen("/etc/dinit.d/tg-ws-proxy", "w");
        if (f) {
            fprintf(f, "type = bgprocess\ncommand = %s 8080 --daemon\n", exe_path);
            fprintf(f, "run-as = %s\nrestart = true\nsmooth-recovery = true\n", user);
            fclose(f);
            system("dinitctl enable tg-ws-proxy");
            system("dinitctl start tg-ws-proxy");
        }
    }
    printf("[SUCCESS] Service installed!\n");
    exit(0);
}

void load_or_generate_secret(char *secret_out) {
    FILE *f = fopen(CONFIG_FILE, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            char *pos = strstr(line, "\"secret\": \"");
            if (pos) {
                pos += 11;
                char *end = strchr(pos, '"');
                if (end) {
                    *end = '\0';
                    strcpy(secret_out, pos);
                    fclose(f);
                    return;
                }
            }
        }
        fclose(f);
    }

    unsigned char rand_bytes[16];
    RAND_bytes(rand_bytes, 16);
    strcpy(secret_out, "ee");
    for (int i = 0; i < 16; i++) {
        sprintf(secret_out + 2 + i * 2, "%02x", rand_bytes[i]);
    }
    f = fopen(CONFIG_FILE, "w");
    if (f) {
        fprintf(f, "{\n  \"secret\": \"%s\"\n}\n", secret_out);
        fclose(f);
    }
}

// Обработка SOCKS5 соединений
void handle_socks5(int client_fd) {
    unsigned char buf[BUFFER_SIZE];
    proxy_log(0, "[INFO] SOCKS5 Protocol Selected.\n");

    if (recv(client_fd, buf, 3, 0) <= 0) { close(client_fd); return; }
    unsigned char auth_reply[2] = {0x05, 0x00};
    send(client_fd, auth_reply, 2, 0);

    if (recv(client_fd, buf, 4, 0) <= 0 || buf[1] != 0x01) { close(client_fd); return; }

    char target_host[256] = {0};
    int target_port = 0;
    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;

    if (buf[3] == 0x01) {
        if (recv(client_fd, buf + 4, 6, 0) <= 0) { close(client_fd); return; }
        memcpy(&target_addr.sin_addr.s_addr, buf + 4, 4);
        target_port = (buf[8] << 8) | buf[9];
    } else if (buf[3] == 0x03) {
        unsigned char len;
        if (recv(client_fd, &len, 1, 0) <= 0) { close(client_fd); return; }
        if (recv(client_fd, target_host, len, 0) <= 0) { close(client_fd); return; }
        target_host[len] = '\0';
        unsigned char port_buf[2];
        if (recv(client_fd, port_buf, 2, 0) <= 0) { close(client_fd); return; }
        target_port = (port_buf[0] << 8) | port_buf[1];
        struct hostent *host = gethostbyname(target_host);
        if (!host) { close(client_fd); return; }
        memcpy(&target_addr.sin_addr.s_addr, host->h_addr, host->h_length);
    }
    target_addr.sin_port = htons(target_port);

    int target_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(target_fd, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0) {
        close(target_fd); close(client_fd); return;
    }

    unsigned char success_reply[10] = {0x05, 0x00, 0x00, 0x01, 0,0,0,0, 0,0};
    send(client_fd, success_reply, 10, 0);

    fd_set readfds;
    int max_fd = (client_fd > target_fd) ? client_fd : target_fd;
    ssize_t bytes_read;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);
        FD_SET(target_fd, &readfds);

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue; break;
        }

        if (FD_ISSET(client_fd, &readfds)) {
            if ((bytes_read = recv(client_fd, buf, sizeof(buf), 0)) <= 0) break;
            if (send(target_fd, buf, bytes_read, 0) < 0) break;
        }
        if (FD_ISSET(target_fd, &readfds)) {
            if ((bytes_read = recv(target_fd, buf, sizeof(buf), 0)) <= 0) break;
            if (send(client_fd, buf, bytes_read, 0) < 0) break;
        }
    }
    close(target_fd);
    close(client_fd);
}

// Обработка MTProto FakeTLS через WSS
void handle_mtproto_wss(int client_fd) {
    unsigned char buf[BUFFER_SIZE];
    
    int rand_idx = rand() % CFPROXY_COUNT;
    char target_host[256];
    decode_domain(CFPROXY_ENC[rand_idx], target_host);
    
    proxy_log(0, "[INFO] MTProto Selected. WSS routing to worker: %s\n", target_host);

    SSL *wss = ws_connect(target_host, 443, TARGET_PATH);
    if (!wss) {
        proxy_log(1, "[ERROR] Failed to establish WSS tunnel to %s\n", target_host);
        close(client_fd);
        return;
    }

    int wss_fd = SSL_get_fd(wss);
    fd_set readfds;
    int max_fd = (client_fd > wss_fd) ? client_fd : wss_fd;
    ssize_t bytes_read;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(client_fd, &readfds);
        FD_SET(wss_fd, &readfds);

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue; break;
        }

        if (FD_ISSET(client_fd, &readfds)) {
            if ((bytes_read = recv(client_fd, buf, sizeof(buf), 0)) <= 0) break;
            if (ws_send_frame(wss, buf, bytes_read) <= 0) break;
        }

        if (FD_ISSET(wss_fd, &readfds)) {
            if ((bytes_read = ws_recv_frame(wss, buf, sizeof(buf))) <= 0) break;
            if (send(client_fd, buf, bytes_read, 0) < 0) break;
        }
    }
    ws_close(wss);
    close(client_fd);
}

// Основной хэндлер, который определяет протокол (Multiplexer)
void *handle_client(void *client_socket_ptr) {
    int client_fd = *((int *)client_socket_ptr);
    free(client_socket_ptr);
    
    unsigned char first_byte;
    // Считываем 1 байт не удаляя его из буфера (PEEK)
    if (recv(client_fd, &first_byte, 1, MSG_PEEK) <= 0) {
        close(client_fd);
        return NULL;
    }

    if (first_byte == 0x05) {
        handle_socks5(client_fd);
    } else {
        handle_mtproto_wss(client_fd);
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int port = DEFAULT_PORT;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--autostart") == 0) install_autostart(argc, argv);
        if (strcmp(argv[i], "--daemon") == 0) run_as_daemon = 1;
    }

    if (run_as_daemon) {
        if (daemon(1, 0) < 0) exit(EXIT_FAILURE);
        openlog("tg-ws-proxy", LOG_PID | LOG_CONS, LOG_DAEMON);
    }

    if (argc > 1 && argv[1][0] != '-') port = atoi(argv[1]);

    char secret[64];
    load_or_generate_secret(secret);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) exit(EXIT_FAILURE);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) exit(EXIT_FAILURE);
    if (listen(server_fd, 100) < 0) exit(EXIT_FAILURE);

    proxy_log(0, "====================================================\n");
    proxy_log(0, "Telegram Multiplexed Proxy (C Version) on port %d\n", port);
    proxy_log(0, "1. MTProto Proxy Link:\n   tg://proxy?server=127.0.0.1&port=%d&secret=%s\n", port, secret);
    proxy_log(0, "2. SOCKS5 Proxy Link:\n   tg://socks?server=127.0.0.1&port=%d\n", port);
    proxy_log(0, "====================================================\n");

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) continue;
        int *new_sock_ptr = malloc(sizeof(int));
        *new_sock_ptr = new_socket;
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)new_sock_ptr) != 0) {
            free(new_sock_ptr);
            close(new_socket);
        } else {
            pthread_detach(thread_id);
        }
    }
    if (run_as_daemon) closelog();
    close(server_fd);
    return 0;
}
