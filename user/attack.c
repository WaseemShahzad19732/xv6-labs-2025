#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    char *p;
    int found = 0;

    // Allocate up to 100 pages (each PGSIZE = 4096 bytes)
    for (int i = 0; i < 100; i++) {
        p = sbrk(PGSIZE);
        // Check if sbrk failed (returns near -1)
        if ((uint64)p >= 0xffffffffffff0000)
            break;

        // Scan every byte in the page for a secret
        for (int j = 0; j <= PGSIZE - 20; j++) {
            char *candidate = &p[j];
            int len = 0;

            // Check for 5–20 alphanumeric characters
            while (len < 20) {
                char c = candidate[len];
                if (c == 0) {
                    break; // null terminator found
                }
                if ((c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9')) {
                    len++;
                } else {
                    break; // invalid character
                }
            }

            // Valid secret: 5–20 chars + null-terminated
            if (len >= 5 && len <= 20 && candidate[len] == 0) {
                printf("%s\n", candidate);
                found = 1;
                goto done;
            }
        }
    }

done:
    if (!found)
        printf("no secret found\n");
    exit(0);
}
