#include "ws_client.h"
#include <stdio.h>
int main() {
    SSL *wss = ws_connect("149.154.167.50", 443, "sprinthost.ru", "nebally.co.uk", "/apiws");
    if (wss) printf("SUCCESS\n");
    else printf("FAILED\n");
    return 0;
}
