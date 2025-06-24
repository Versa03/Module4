#include "std_lib.h"

int div(int a, int b) {
    int result = 0;
    if (b == 0) return -1;
    while (a >= b) {
        a = a - b;
        result++;
    }
    return result;
}

int mod(int a, int b) {
    if (b == 0) return -1;
    while (a >= b) {
        a = a - b;
    }
    return a;
}

void memcpy(byte* src, byte* dst, unsigned int size) {
    unsigned int i;
    for (i = 0; i < size; i++) {
        dst[i] = src[i];
    }
}

unsigned int strlen(char* str) {
    unsigned int i = 0;
    while (str[i] != '\0') {
        i++;
    }
    return i;
}

/* Versi strcmp yang lebih aman */
bool strcmp(char* str1, char* str2) {
    int i = 0;
    while (str1[i] != '\0' && str2[i] != '\0') {
        if (str1[i] != str2[i]) {
            return false;
        }
        i++;
    }
    /* Pastikan kedua string berakhir di titik yang sama */
    if (str1[i] == '\0' && str2[i] == '\0') {
        return true;
    }
    return false;
}

/* Versi strcpy yang lebih aman */
void strcpy(char* src, char* dst) {
    int i = 0;
    while (src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void clear(byte* buf, unsigned int size) {
    unsigned int i;
    for (i = 0; i < size; i++) {
        buf[i] = 0;
    }
}
