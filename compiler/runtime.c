// laurelang compiler
// (c) Arseny Kriuchkov, 2022

#include "compiler.h"

bool get_bit(unsigned char byte, uint pos);

bool read_bits(Bitstream *bits, uint count, bool *buf) {
    for (uint i = 0; i < count; i++) {
        int bit = bitstream_read_bit(bits);
        if (bit == EOF) return false;
        buf[i] = bit ? true : false;
    }
    return true;
}

void generate_signature(bool *signature) {
    assert(SIGNATURE_LENGTH <= strlen(SIGNATURE));
    for (uint i = 0; i < SIGNATURE_LENGTH; i++) {
        for (uint j = 0; j < SIGNATURE_CHARBITS; j++) {
            signature[(i * 4) + j] = get_bit(SIGNATURE[i], j);
        }
    }
}

bool signature_eq(bool *sign1, bool *sign2) {
    for (uint i = 0; i < (SIGNATURE_LENGTH * SIGNATURE_CHARBITS); i++) {
        if (sign1[i] != sign2[i]) return false;
    }
    return true;
}

void print_signature(bool *sign) {
    for (uint i = 0; i < (SIGNATURE_LENGTH * SIGNATURE_CHARBITS); i++) {
        printf("%d", sign[i]);
    }
}

void align(Bitstream *bs) {
    bitstream_load_byte(bs);
}

void laure_compiler_consult_bytecode(Bitstream *bits) {

    // SIGNATURE CHECK
    align(bits);
    debug("checking signature\n");
    bool real_signature[SIGNATURE_LENGTH * SIGNATURE_CHARBITS];
    bool has_signature = read_bits(bits, SIGNATURE_LENGTH * SIGNATURE_CHARBITS, real_signature);
    if (! has_signature)
        printf("    %spanic%s: failure attempting to read signature\n", RED_COLOR, NO_COLOR);
    bool valid_signature[SIGNATURE_LENGTH * SIGNATURE_CHARBITS];
    generate_signature(valid_signature);
    if (! signature_eq(real_signature, valid_signature)) {
        printf("    %swrong signature%s!\n", RED_COLOR, NO_COLOR);
        printf("        expected %s", BOLD_WHITE);
        print_signature(valid_signature);
        printf("%s\n", NO_COLOR);
        printf("        got      %s", BOLD_WHITE);
        print_signature(real_signature);
        printf("%s\n", NO_COLOR);
        return;
    }
    debug("confirmed signature\n");

    // CONSULTING EXPRESSIONS
    // ...
}