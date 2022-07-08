// laurelang compiler
// (c) Arseny Kriuchkov, 2022

#include "compiler.h"
#include <limits.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

Name VARIABLE_IDS[ID_MAX];
uint VARIABLES_SIGNED = 0;
unsigned char SIGNATURE[] = "laurelang";

string *KNOWN_NAMES_DYNAMIC[ID_MAX];
uint KNOWN_NAMES_DYNAMIC_N = 0;

uint ID_BITS = 0;

bool get_bit(unsigned char byte, uint pos) {
    assert(pos >= 0 && pos <= 7);
    return (byte >> pos) & 0x1;
}

void bits_inc(bool bits[], uint n) {
    for (uint i = 0; i < n; i++) {
        uint j = n - i - 1;
        if (! bits[j]) {
            bits[j] = 1;
            for (uint k = j + 1; k < n; k++)
                bits[k] = 0;
            break;
        }
    }
}

void calc_bits(int value, bool bits[], uint n) {
    memset(bits, 0, sizeof(bool) * n);
    while (value > 0) {
        bits_inc(bits, n);
        value--;
    }
}

void write_header(Bitstream *bs, int header) {
    bool bits[HEADER_BITS];
    calc_bits(header, bits, HEADER_BITS);
    for (uint i = 0; i < HEADER_BITS; i++) {
        bool bit = bits[i];
        bitstream_write_bit(bs, bit);
    }
}

bool check_uint_fit(uint i, uint count_bits) {
    return i <= CHAR_MAX && i <= pow(2, count_bits);
}

void add_known_name(string name) {
    assert(KNOWN_NAMES_DYNAMIC_N < ID_MAX);
    KNOWN_NAMES_DYNAMIC[KNOWN_NAMES_DYNAMIC_N++] = name;
}

bool is_known_name(string name) {
    for (uint i = 0; i < KNOWN_NAMES_DYNAMIC_N; i++) {
        if (str_eq(KNOWN_NAMES_DYNAMIC[i], name)) return true;
    }
    return false;
}

void write_count_bits_from_uint(Bitstream *bs, uint integer, uint count_bits) {
    // integer fit checks
    assert(integer <= pow(2, count_bits));
    assert(integer <= CHAR_MAX);

    bool bits[64]; // max
    calc_bits(integer, bits, count_bits);

    for (uint i = 0; i < count_bits; i++) {
        bool bit = bits[i];
        bitstream_write_bit(bs, bit);
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

bool bitstream_load_byte(Bitstream *bs) {
    int byte = fgetc(bs->stream);
    if (byte == EOF)
        return false;
    bs->buf = byte;
    bs->idx = 0;
    return true;
}


int bitstream_read_bit(Bitstream *bs) {
    if (bs->idx == CHAR_BIT && ! bitstream_load_byte(bs))
        return EOF;
    return ((0x80 >> bs->idx++) & bs->buf) == 0 ? 0 : 1;
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
        printf("%spanic%s: too many variables created (cannot fit in ID_COUNT)\n", RED_COLOR, NO_COLOR);
        return false;
    }
    // create an id
    Name new_name;
    new_name.s    = strdup(var_name);
    new_name.flag = flag;

    VARIABLE_IDS[VARIABLES_SIGNED++] = new_name;
    uint id = VARIABLES_SIGNED - 1;
    write_count_bits_from_uint(bits, id, ID_BITS);
    return true;
}

bool write_name(Bitstream *bits, string s) {
    if (strlen(s) > CHAR_MAX) {
        printf("%spanic%s: too long predicate name\n", RED_COLOR, NO_COLOR);
        return false;
    }
    unsigned char length = (unsigned char) strlen(s);
    write_bits(bits, length, 8);
    for (unsigned char idx = 0; idx < length; idx++) {
        unsigned char c = s[idx];
        bool cbits[8];
        calc_bits((int)c, cbits, 8);
        for (uint j = 0; j < 8; j++) bitstream_write_bit(bits, cbits[j]);
    }
    return true;
}

void write_give_id(Bitstream *bits, string name) {
    write_header(bits, CH_give_id);
    write_name_ID(bits, name, 0);
    write_name(bits, name);
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
                    printf("%spanic%s: too big nesting\n", RED_COLOR, NO_COLOR);
                    return false;
                }
                write_count_bits_from_uint(bits, expr->flag, COUNT_BITS_NESTING);
            }
            break;
        }
        case let_set: {
            if (expr->is_header) {
                laure_expression_t *ptr;
                EXPSET_ITER(expr->ba->set, ptr, {
                    ptr->is_header = true;
                    bool result = compile_expression_with_bitstream(ptr, bits);
                    if (! result) return false;
                });
                return true;
            } else {
                if (expr->flag) {
                    // isolated set
                    write_header(bits, CH_isol_start);
                }
                // append expressions
                laure_expression_t *ptr;
                EXPSET_ITER(expr->ba->set, ptr, {
                    bool result = compile_expression_with_bitstream(ptr, bits);
                    if (! result) return false;
                });
                if (expr->flag) {
                    write_header(bits, CH_endblock);
                }
                return true;
            }
        }
        case let_cut: {
            write_header(bits, CH_cut);
            break;
        }
        case let_pred_call: {
            if (! is_known_name(expr->s)) {
                write_give_id(bits, expr->s);
            }
            write_header(bits, CH_call);
            write_name_ID(bits, expr->s, 0);
            laure_expression_t *ptr;
            EXPSET_ITER(expr->ba->set, ptr, {
                bool result = compile_expression_with_bitstream(ptr, bits);
                if (! result) return false;
            });
            write_header(bits, CH_endblock);
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
                bool is_abstract_template = PREDFLAG_IS_TEMPLATE(expr->flag);
                bool has_response = expr->ba->has_resp;
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
                write_header(bits, CH_preddecl);
                int id = check_name_exists(expr->s);
                if (id < 0) {
                    printf("%spanic%s: header for predicate %s is undefined\n", RED_COLOR, NO_COLOR, expr->s);
                    return false;
                }
                write_name_ID(bits, expr->s, false);
                Name name = get_name((ID)id);
                laure_expression_t *ptr = 0;
                uint args_len = laure_expression_get_count(expr->ba->set) - expr->ba->body_len;
                if (expr->ba->has_resp) args_len--;

                uint i = 0;
                EXPSET_ITER(expr->ba->set, ptr, {
                    i++;
                    if (i == args_len) write_header(bits, CH_endblock);
                    bool result = compile_expression_with_bitstream(ptr, bits);
                    if (! result) return false;
                });
                write_header(bits, CH_endblock);
                break;
            }
        }
        case let_image: {
            laure_expression_t *expr1 = expr->ba->set->expression;
            laure_expression_t *expr2 = expr->ba->set->next->expression;
            if (expr1->t == let_var && expr2->t == let_var) {
                write_header(bits, CH_img);
                write_name_ID(bits, expr1->s, 0);
                write_name_ID(bits, expr2->s, 0);
                break;
            } else {
                write_header(bits, CH_imgNS);
                bool result;
                result = compile_expression_with_bitstream(expr1, bits);
                if (! result) return false;
                result = compile_expression_with_bitstream(expr2, bits);
                return result;
            }
        }
        case let_assert: {
            laure_expression_t *expr1 = expr->ba->set->expression;
            laure_expression_t *expr2 = expr->ba->set->next->expression;
            if (expr1->t != let_var) {
                printf("%spanic%s: assertion left value must be variable\n", RED_COLOR, NO_COLOR);
                return false;
            }
            if (expr2->t == let_var) {
                write_header(bits, CH_assertV2V);
                write_name_ID(bits, expr1->s, 0);
                write_name_ID(bits, expr2->s, 0);
                break;
            } else {
                write_header(bits, CH_assertV2VAL);
                write_name_ID(bits, expr1->s, 0);
                return compile_expression_with_bitstream(expr2, bits);
            }
        }
        case let_custom: {
            write_header(bits, CH_data);
            write_name(bits, expr->s);
            break;
        }
        case let_imply: {
            write_header(bits, CH_implication);
            laure_expression_t *ptr;
            EXPSET_ITER(expr->ba->set->expression->ba->set, ptr, {
                bool result = compile_expression_with_bitstream(ptr, bits);
                if (! result) return false;
            });
            write_header(bits, CH_endblock);
            EXPSET_ITER(expr->ba->set->next, ptr, {
                bool result = compile_expression_with_bitstream(ptr, bits);
                if (! result) return false;
            });
            write_header(bits, CH_endblock);
            break;
        }
        default: {
            printf("%swarning%s: compiler has no instruction for %s expression (%s)\n", YELLOW_COLOR, NO_COLOR, EXPT_NAMES[expr->t], expr->fullstring);
            break;
        }
    }
    return true;
}

void write_signature(FILE *file) {
    assert(SIGNATURE_LENGTH <= strlen(SIGNATURE));
    Bitstream *bits = bitstream_new(file);
    for (uint i = 0; i < SIGNATURE_LENGTH; i++) {
        write_bits(bits, SIGNATURE[i], SIGNATURE_CHARBITS);
    }
    bitstream_flush(bits);
}

bool laure_compiler_compile_expression(
    laure_expression_t *expr,
    FILE *writable_stream
) {
    Bitstream *bits = bitstream_new(writable_stream);
    laure_expression_set *composed = laure_expression_compose_one(expr);
    laure_expression_t *ptr;
    EXPSET_ITER(composed, ptr, {
        if (! compile_expression_with_bitstream(ptr, bits)) {
            printf("     > compilation failure\n");
            return false;
        }
        bitstream_flush(bits);
    });
    return true;
}