#include "defines.h"
#include "serial.h"
#include "lib.h"

////////////////////////////////////////
// メモリ関連
////////////////////////////////////////

// メモリを特定パターンで埋める。
void *memset(void *b, int c, long len) {
  char *p;
  for (p = b; len > 0; len--)
    *(p++) = c;
  return b;
}

// メモリのコピー。
void *memcpy(void *dst, const void *src, long len) {
  char *d = dst;
  const char *s = src;
  for (; len > 0; len--)
    *(d++) = *(s++);
  return dst;
}

// メモリの比較。
int memcmp(const void *b1, const void *b2, long len) {
  const char *p1 = b1, *p2 = b2;
  for (; len > 0; len--) {
    if (*p1 != *p2)
      return (*p1 > *p2) ? 1 : -1;
    p1++;
    p2++;
  }
  return 0;
}

////////////////////////////////////////
// 文字列関連
////////////////////////////////////////

// 文字列の長さを返す。
int strlen(const char *s) {
  int len;
  for (len = 0; *s; s++, len++)
    ;
  return len;
}

// 文字列のコピー。
char *strcpy(char *dst, const char *src) {
  char *d = dst;
  for (;; dst++, src++) {
    *dst = *src;
    if (!*src) break;
  }
  return d;
}

// 文字列の比較。
int strcmp(const char *s1, const char *s2) {
  while (*s1 || *s2) {
    if (*s1 != *s2)
      return (*s1 > *s2) ? 1 : -1;
    s1++;
    s2++;
  }
  return 0;
}

// 長さ制限有りで文字列の比較。
int strncmp(const char *s1, const char *s2, int len) {
  while ((*s1 || *s2) && len > 0) {
    if (*s1 != *s2)
      return (*s1 > *s2) ? 1 : -1;
    s1++;
    s2++;
    len--;
  }
  return 0;
}

////////////////////////////////////////
// シリアル送信
////////////////////////////////////////

// 1文字送信。
int putc(unsigned char c) {
  if (c == '\n')
    serial_send_byte(SERIAL_DEFAULT_DEVICE, '\r');
  return serial_send_byte(SERIAL_DEFAULT_DEVICE, c);
}

// 文字列送信
int puts(unsigned char *str) {
  while (*str)
    putc(*(str++));
  return 0;
}

// 16進数の数値送信。
int putxval(unsigned long value, int column) {
  char buf[9];
  char *p;

  p = buf + sizeof(buf) - 1; // 下の桁から処理する。
  *(p--) = '\0';

  if (!value && !column)
    column++;

  while (value || column) {
    // value & 0xf で下位4ビット(0-15)を取り出し、それを16進数の文字にマッピングする。
    *(p--) = "0123456789abcdef"[value & 0xf];
    // 4ビット右シフト
    value >>= 4;
    // 桁指定がある場合はカウント。
    if (column) column--;
  }

  // 前述のループから抜ける時、余分にpをデクリメントしているため。
  puts(p + 1);

  return 0;
}
