// laurelang compiler
// (c) Arseny Kriuchkov, 2022

#include "compiler.h"
#include <limits.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

typedef unsigned char ID;

Name VARIABLE_IDS[ID_MAX];
uint VARIABLES_SIGNED = 0;

uint ID_BITS = 0;

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

bool bitstream_flush(Bitstream *bs) {
    if (bs->idx > 0) {
        if (fputc(bs->buf, bs->stream) == EOF)
            return false;
        bs->buf = 0;
        bs->idx = 0;
    }
    return true;
}

bool bitstream_write_bit(Bitstream *bs, bool bit) {
    if (bs->idx == CHAR_BIT && ! bitstream_flush(bs))
        return false;
    if (bit)
        bs->buf |= (0x80 >> bs->idx);
    bs->idx++;
    return true;
}

Name get_name(ID id) {
    return VARIABLE_IDS[id];
}

/*
returns -1 if undefined
else ID 
*/
int check_name_exists(string name) {
    for (uint i = 0; i < VARIABLES_SIGNED; i++)
        if (str_eq(VARIABLE_IDS[i].s, name))
            return i;
    return -1;
}

bool write_name_ID(Bitstream *bits, string var_name, bool flag) {
    for (uint i = 0; i < VARIABLES_SIGNED; i++)
        if (str_eq(VARIABLE_IDS[i].s, var_name)) {
            write_count_bits_from_uint(bits, i, ID_BITS);
            return true;
        }
    if (VARIABLES_SIGNED >= ID_MAX) {
        printf("panic: too many variables created (cannot fit in ID_COUNT)\n");
        return false;
    }
    // create an id
    Name new_name;
    new_name.s = strdup(var_name);
    new_name.flag = flag;

    VARIABLE_IDS[VARIABLES_SIGNED++] = new_name;
    uint id = VARIABLES_SIGNED - 1;
    write_count_bits_from_uint(bits, id, ID_BITS);
    return true;
}

bool write_name(Bitstream *bits, string s) {
    if (strlen(s) > CHAR_MAX) {
        printf("panic: too long predicate name\n");
        return false;
    }
    unsigned char length = (unsigned char) strlen(s);
    write_bits(bits, length, CHAR_BIT);
    for (unsigned char idx = 0; idx < length; idx++) {
        write_bits(bits, s[idx], CHAR_BIT);
    }
    return true;
}

void init_settings() {
    if (! ID_BITS)
        ID_BITS = log2(ID_MAX);
}

bool compile_expression_with_bitstream(
    laure_expression_t *expr, 
    Bitstream *bits
) {
    init_settings();
    switch (expr->t) {
        case let_var: 
        case let_unify: {
            if (expr->t == let_var) {
                write_header(bits, CH_var);
            } else if (expr->t == let_unify)
                write_header(bits, CH_unify);
            write_name_ID(bits, expr->s, false);
            if (expr->flag > 0) {
                if (! check_uint_fit(expr->flag, COUNT_BITS_NESTING)) {
                    printf("panic: too big nesting\n");
                    return false;
                }
                write_count_bits_from_uint(bits, expr->flag, COUNT_BITS_NESTING);
            }
            break;
        }
        case let_pred:
        case let_constraint: {
            if (expr->is_header) {
                if (expr->t == let_pred)
                    write_header(bits, CH_preddeclHead);
                else
                    write_header(bits, CH_cnstrHead);
                
                // predicate name
                write_name(bits, expr->s);
                write_name_ID(bits, expr->s, expr->ba->has_resp);
                // flag bits
                bool is_primitive = PREDFLAG_IS_PRIMITIVE(expr->flag);
                bool is_abstract_template = PREDFLAG_IS_TEMPLATE(expr->flag);
                bool has_response = expr->ba->has_resp;
                bitstream_write_bit(bits, is_primitive);
                bitstream_write_bit(bits, is_abstract_template);
                bitstream_write_bit(bits, has_response);
                
                // write linked expressions
                laure_expression_t *ptr = 0;
                EXPSET_ITER(expr->ba->set, ptr, {
                    bool result = compile_expression_with_bitstream(ptr, bits);
                    if (! result) return false;
                });
                write_header(bits, CH_endblock);
                break;
            } else {
                int id = check_name_exists(expr->s);
                if (id < 0) {
                    printf("panic: header for predicate %s is undefined\n", expr->s);
                    return false;
                }
                Name name = get_name((ID)id);
                laure_expression_t *ptr = 0;
                EXPSET_ITER(expr->ba->set, ptr, {
                    bool result = compile_expression_with_bitstream(ptr, bits);
                    if (! result) return false;
                });
                break;
            }
        }
        default: break;
    }
    return true;
}

bool laure_compile_expression(
    laure_expression_t *expr,
    FILE *writable_stream
) {
    Bitstream *bits = bitstream_new(writable_stream);
    if (compile_expression_with_bitstream(expr, bits)) {
        bitstream_flush(bits);
        return true;
    }
    printf("compilation failure\n");
    return false;
}