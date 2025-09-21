// user/memdump.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

// --- helpers ---
static uint64 read_le(const char *p, int nbytes) {
  uint64 v = 0;
  for (int i = 0; i < nbytes; i++) {
    v |= ((uint64)(uchar)p[i]) << (8*i);
  }
  return v;
}

static void print_hex(uint64 x) {
  if (x == 0) { printf("0\n"); return; }
  char digits[] = "0123456789abcdef";
  char buf[32];
  int i = 0;
  while (x) { buf[i++] = digits[x & 0xF]; x >>= 4; }
  char out[32];
  int j = 0;
  while (i > 0) out[j++] = buf[--i];
  out[j] = 0;
  printf("%s\n", out);
}

// --- main memdump function ---
void
memdump(char *fmt, char *data)
{
  for (char *f = fmt; *f; f++) {
    switch (*f) {
      case 'i': { // 4 bytes as int
        uint64 x = read_le(data, 4);
        int v = (int)(x & 0xFFFFFFFFULL);
        printf("%d\n", v);
        data += 4;
        break;
      }
      case 'p': { // 8 bytes as pointer/int
        uint64 x = read_le(data, 8);
        uint32 low = (uint32)(x & 0xFFFFFFFFULL);
        print_hex(low);   // print low 32 bits
        print_hex(x);     // print full 64 bits
        data += 8;
        break;
      }
      case 'h': { // 2 bytes as short
        uint64 x = read_le(data, 2);
        int v = (int)(x & 0xFFFFULL);
        printf("%d\n", v);
        data += 2;
        break;
      }
      case 'c': { // 1 byte as char
        printf("%c\n", *data);
        data += 1;
        break;
      }
      case 's': { // pointer to C string
        uint64 addr = read_le(data, 8);
        if (addr == 0) {
          printf("(null)\n");
        } else {
          printf("%s\n", (char*)addr);
        }
        data += 8;
        break;
      }
      case 'S': { // inline C string
        printf("%s\n", data);
        while (*data) data++;
        data++;
        break;
      }
      default:
        break;
    }
  }
}

// --- driver program ---
int
main(int argc, char *argv[])
{
  if (argc == 1) {
    // Example 1
    printf("Example 1:\n");
    char ex1[8];
    int v1 = 61810, v2 = 2025;
    memmove(ex1, &v1, 4);
    memmove(ex1+4, &v2, 4);
    memdump("ii", ex1);

    // Example 2
    printf("Example 2:\n");
    char *s2 = "a string";
    memdump("s", (char*)&s2);

    // Example 3
    printf("Example 3:\n");
    memdump("S", "another");

    // Example 4
    printf("Example 4:\n");
    char ex4[4+4+4+1+6];
    int off = 0;
    ex4[off++] = 'B'; ex4[off++] = 'D'; ex4[off++] = '0'; ex4[off++] = 0;
    int n1 = 1819438967; memmove(&ex4[off], &n1, 4); off+=4;
    int n2 = 100;        memmove(&ex4[off], &n2, 4); off+=4;
    ex4[off++] = 'z';
    ex4[off++]='x'; ex4[off++]='y'; ex4[off++]='z'; ex4[off++]='z'; ex4[off++]='y'; ex4[off++]=0;
    memdump("SiicS", ex4);

    // Example 5
    printf("Example 5:\n");
    char ex5[6+5];
    off = 0;
    ex5[off++]='h'; ex5[off++]='e'; ex5[off++]='l'; ex5[off++]='l'; ex5[off++]='o'; ex5[off++]=0;
    ex5[off++]='w'; ex5[off++]='o'; ex5[off++]='r'; ex5[off++]='l'; ex5[off++]='d';
    memdump("Sccccc", ex5);

    exit(0);
  }

  // Read stdin into buffer
  char buf[4096];
  int n, tot = 0;
  while ((n = read(0, buf+tot, sizeof(buf)-tot)) > 0) tot += n;
  memdump(argv[1], buf);
  exit(0);
}
