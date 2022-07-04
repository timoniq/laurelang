// laurelang compiler
// (c) Arseny Kriuchkov, 2022

#pragma once

#include "compiler.h"
#include <limits.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "../src/expr.h"

typedef unsigned char ID;
typedef enum {false, true} bool;

string VARIABLE_IDS[CHAR_BIT];
unsigned char VARIABLES_SIGNED = 0;

typedef struct Bitstream {
    FILE *stream;
    unsigned char buf;
    unsigned int idx;
} Bitstream;

bool get_bit(unsigned char byte, uint pos) {
    assert(pos >= 0 && pos <= 7);
    return (byte >> pos) & 0x1;
}

void write_header(Bitstream *bs, unsigned char header) {
    for (uint i = 0; i < HEADER_BITS; i++)
        bitstream_write_bit(bs, get_bit(header, i));
}

bool check_uint_fit(uint i, uint count_bits) {
    return i <= CHAR_MAX && i <= pow(2, count_bits);
}

void write_count_bits_from_uint(Bitstream *bs, uint integer, uint count_bits) {
    // integer fit checks
    assert(integer <= pow(2, count_bits));
    assert(integer <= CHAR_MAX);

    unsigned char byte = (unsigned char)integer;
    for (uint i = 0; i < count_bits; i++) {
        uint pos = CHAR_BIT - i - 1;
        bitstream_write_bit(bs, get_bit(byte, pos));
    } 
}

void write_bits(Bitstream *bs, unsigned char byte, uint count) {
    assert(count <= 8);
    for (uint i = 0; i < count; i++)
        bitstream_write_bit(bs, get_bit(byte, i));
}

Bitstream *bitstream_new(FILE *stream) {
    Bitstream *bs = malloc(sizeof(Bitstream));
    bs->stream = stream;
    bs->idx = 0;
    bs->buf = 0;
    return bs;
}

bool bitstream_write_bit(Bitstream *bs, bool bit) {
    if (bs->idx == CHAR_BIT && ! bitstream_flush(bs))
        return false;
    if (bit)
        bs->buf |= (0x80 >> bs->idx);
    bs->idx++;
    return true;
}

bool bitstream_flush(Bitstream *bs) {
    if (bs->idx > 0) {
        if (fputc(bs->buf, bs->stream) == EOF)
            return false;
        bs->buf = 0;
        bs->idx = 0;
    }
    return true;
}

unsigned char search_var(string var_name) {
    for (unsigned char i = 0; i < VARIABLES_SIGNED; i++)
        if (str_eq(VARIABLE_IDS[i], var_name))
            return i;
    // create an id
    VARIABLE_IDS[VARIABLES_SIGNED++] = strdup(var_name);
    return VARIABLES_SIGNED - 1;
}

void compile_expression(
    laure_expression_t *expr,
    FILE *writable_stream
) {
    Bitstream *bits = bitstream_new(writable_stream);
    switch (expr->t) {
        case let_var: 
        case let_unify: {
            if (expr->t == let_var) {
                write_header(bits, CH_var);
            } else if (expr->t == let_unify)
                write_header(bits, CH_unify);
            ID var_id = search_var(expr->s);
            write_bits(bits, var_id, CHAR_BIT);
            if (expr->flag > 0) {
                if (! check_uint_fit(expr->flag, COUNT_BITS_NESTING)) {
                    printf("panic: too big nesting\n");
                    return;
                }
                write_count_bits_from_uint(bits, expr->flag, COUNT_BITS_NESTING);
            }
            break;
        }
    }
    bitstream_flush(bits);
}