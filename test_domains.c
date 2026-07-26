#include <stdio.h>
#include <string.h>

void decode_domain(const char* s, char* out) {
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

int main() {
    const char* CFPROXY_ENC[] = {
        "virkgj.com", "vmmzovy.com", "mkuosckvso.com", "zaewayzmplad.com",
        "wzxxzkx.com", "bchqihv.com", "aylnyv.com", "lsvo.com",
        "vsvbspg.com", "wkkxq.com", "wlhuv.com", "mshvuv.com",
        "thzjo.com", "qhsld.com", "ovzsl.com", "zhukv.com",
        "khevn.com", "xvdls.com", "clyzi.com", "rhzvt.com",
        "whtlx.com", "jyltl.com"
    };
    for(int i=0; i<22; i++){
        char out[256];
        decode_domain(CFPROXY_ENC[i], out);
        printf("%s\n", out);
    }
    return 0;
}
