#include <kernel/types.h>

size_t strlen(const char *str) {
    if (!str) return 0;
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    if (!s1 || !s2) return s1 == s2 ? 0 : (s1 ? 1 : -1);
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    if (!s1 || !s2 || n == 0) return 0;
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i] || !s1[i] || !s2[i]) {
            return s1[i] - s2[i];
        }
    }
    return 0;
}

char *strcpy(char *dest, const char *src) {
    if (!dest || !src) return dest;
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    if (!dest || !src) return dest;
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    char *d = dest;
    while (*d) d++;
    for (size_t i = 0; i < n && src[i]; i++) {
        *d++ = src[i];
    }
    *d = '\0';
    return dest;
}

char *strchr(const char *s, int c) {
    if (!s) return NULL;
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == '\0') ? (char*)s : NULL;
}

char *strrchr(const char *s, int c) {
    if (!s) return NULL;
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char*)s;
    return (char*)last;
}

void *memcpy(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    uint8_t *d = dest;
    const uint8_t *s = src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    uint8_t *d = dest;
    const uint8_t *s = src;
    
    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    if (!s || n == 0) return s;
    uint8_t *p = s;
    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    if (!s1 || !s2 || n == 0) return 0;
    const uint8_t *p1 = s1;
    const uint8_t *p2 = s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

/* Secure string copy with bounds checking */
size_t strlcpy(char *dest, const char *src, size_t size) {
    if (!dest || !src || size == 0) return 0;
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len >= size) ? size - 1 : src_len;
    
    for (size_t i = 0; i < copy_len; i++) {
        dest[i] = src[i];
    }
    dest[copy_len] = '\0';
    
    return src_len;
}

/* Secure string concatenation with bounds checking */
size_t strlcat(char *dest, const char *src, size_t size) {
    if (!dest || !src || size == 0) return 0;
    
    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);
    
    if (dest_len >= size) return size + src_len;
    
    size_t copy_len = size - dest_len - 1;
    if (src_len < copy_len) {
        copy_len = src_len;
    }
    
    for (size_t i = 0; i < copy_len; i++) {
        dest[dest_len + i] = src[i];
    }
    dest[dest_len + copy_len] = '\0';
    
    return dest_len + src_len;
}

/* Validate string pointer and check for null termination within maxlen */
int str_is_valid(const char *str, size_t maxlen) {
    if (!str) return 0;
    for (size_t i = 0; i < maxlen; i++) {
        if (str[i] == '\0') return 1;
    }
    return 0;
}
