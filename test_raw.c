#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(443);
    serv_addr.sin_addr.s_addr = inet_addr("149.154.167.50");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connect failed\n");
        return 1;
    }
    // send some garbage, see if it drops immediately
    send(sock, "hello", 5, 0);
    char buf[1024];
    int n = recv(sock, buf, sizeof(buf), 0);
    printf("Recv: %d\n", n);
    return 0;
}
