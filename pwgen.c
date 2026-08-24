#include <mcrypto.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <stdio.h>

int main(int ac, char** av) {
    if (ac < 2) errx(1, "not enough args");

    uint8_t hash[32];
    uint8_t salt[16];
    arc4random_buf(salt, 16);

    void* wb = malloc(100000 * 1024);
    if (!wb) {
        err(1, "malloc failed");
    }

    crypto_argon2_config a2cfg = {
        .algorithm = CRYPTO_ARGON2_ID,
        .nb_blocks = 100000,
        .nb_passes = 3,
        .nb_lanes = 1
    };
    crypto_argon2_extras a2ex = {
        .key = NULL,
        .ad = NULL,
        .key_size = 0,
        .ad_size = 0
    };

    crypto_argon2_inputs a2is = {
        .pass = (const uint8_t*)av[1],
        .salt = salt,
        .pass_size = strlen(av[1]),
        .salt_size = 16
    };

    crypto_argon2(hash, 32, wb, a2cfg, a2is, a2ex);

    printf("$argon2id$%d,%d,%d$",
           a2cfg.nb_blocks,
           a2cfg.nb_passes,
           a2cfg.nb_lanes);
    for (int i = 0; i < 16; i++) {
        printf("%02x", salt[i]);
    }
    putchar('$');
    for (int i = 0; i < 32; i++) {
        printf("%02x", hash[i]);
    }
    putchar('\n');

    return 0;
}