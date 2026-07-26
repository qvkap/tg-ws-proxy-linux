#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

static int custom_resolve(const char *hostname, struct in_addr *out_ip) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return -1;
    
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(53);
    
    const char *dns_servers[] = {"1.1.1.1", "8.8.8.8"};
    dest.sin_addr.s_addr = inet_addr(dns_servers[0]);

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    unsigned char buf[512];
    buf[0] = 0x12; buf[1] = 0x34;
    buf[2] = 0x01; buf[3] = 0x00;
    buf[4] = 0x00; buf[5] = 0x01;
    buf[6] = 0x00; buf[7] = 0x00;
    buf[8] = 0x00; buf[9] = 0x00;
    buf[10]= 0x00; buf[11]= 0x00;

    int pos = 12;
    const char *p = hostname;
    while (*p) {
        const char *dot = strchr(p, '.');
        if (!dot) dot = p + strlen(p);
        int len = dot - p;
        buf[pos++] = len;
        memcpy(buf + pos, p, len);
        pos += len;
        if (!*dot) break;
        p = dot + 1;
    }
    buf[pos++] = 0;
    buf[pos++] = 0x00; buf[pos++] = 0x01;
    buf[pos++] = 0x00; buf[pos++] = 0x01;
    
    if (sendto(sock, buf, pos, 0, (struct sockaddr*)&dest, sizeof(dest)) < 0) {
        close(sock);
        return -1;
    }

    socklen_t dest_len = sizeof(dest);
    int res = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&dest, &dest_len);
    close(sock);
    
    if (res < pos + 16) return -1;
    int ancount = (buf[6] << 8) | buf[7];
    if (ancount == 0) return -1;

    int rpos = pos;
    for (int i = 0; i < ancount; i++) {
        if (rpos >= res) return -1;
        if ((buf[rpos] & 0xC0) == 0xC0) {
            rpos += 2;
        } else {
            while (buf[rpos] != 0) {
                rpos += buf[rpos] + 1;
                if (rpos >= res) return -1;
            }
            rpos++;
        }
        if (rpos + 10 > res) return -1;
        int type = (buf[rpos] << 8) | buf[rpos+1];
        rpos += 8;
        int rdlength = (buf[rpos] << 8) | buf[rpos+1];
        rpos += 2;
        
        if (type == 1 && rdlength == 4 && rpos + 4 <= res) {
            memcpy(&out_ip->s_addr, buf + rpos, 4);
            return 0;
        }
        rpos += rdlength;
    }
    return -1;
}

int main() {
    struct in_addr ip;
    if (custom_resolve("onedaychamp.co.uk", &ip) == 0) {
        printf("Resolved to %s\n", inet_ntoa(ip));
    } else {
        printf("Failed to resolve\n");
    }
    return 0;
}
