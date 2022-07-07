// laurelang compiler
// (c) Arseny Kriuchkov, 2022

#include "compiler.h"

control_ctx *CONTEXT = 0;

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

bool zeros(bool bits[], uint l) {
    for (uint i = 0; i < l; i++) {
        if (bits[i] != 0) return false;
    }
    return true;
}

int bitset_toint(bool bits[], uint n) {
    int header = 0;
    while (! zeros(bits, n)) {
        for (uint i = n-1; i >= 0; i--) {
            bool bit = bits[i];
            if (! bit) continue;
            for (uint j = i + 1; j < n; j++) bits[j] = 1;
            bits[i] = 0;
            header++;
            break;
        }
    }
    return header;
}

char bitset_uchar(bool bits[CHAR_BIT]) {
    unsigned char B = 0;
    for (uint i = 0; i < CHAR_BIT; i++) {
        if (bits[i])
            B |= 1UL << i;
    }
    return B;
}

typedef enum consultS {
    consult_fine,
    consult_stoperr,
    consult_endblock
} consultS;

#define CONSULT_EXP_REC(name) \
        bool (*name)(laure_expression_t *expr, void *payload)

bool set_appender(laure_expression_t *expr, laure_expression_set *set) {
    set->expression = expr;
    set->next = 0;
    return true;
}

unsigned char read_uchar(Bitstream *bits) {
    bool byte[CHAR_BIT];
    bool can_read = read_bits(bits, CHAR_BIT, byte);
    if (! can_read) return 0;
    unsigned char c = bitset_uchar(byte);
    return c;
}

string read_name(Bitstream *bits) {
    bool length_byte[8];
    bool can_read = read_bits(bits, CHAR_BIT, length_byte);
    if (! can_read) return 0;
    unsigned char length = bitset_uchar(length_byte);
    string s = malloc(length + 1);
    s[length] = 0;
    for (unsigned char i = 0; i < length; i++) {
        unsigned char c = read_uchar(bits);
        s[i] = c;
    }
    return s;
}

consultS consult_expression(
    laure_session_t *session, 
    Bitstream *bits,
    CONSULT_EXP_REC(rec),
    void *external_payload
) {
    align(bits);
    bool header[HEADER_BITS];
    bool can_read = read_bits(bits, HEADER_BITS, header);
    if (! can_read) return consult_stoperr;
    int h = bitset_toint(header, HEADER_BITS);
    
    printf("%d\n", h);
    switch (h) {
        case CH_endblock: {
            return consult_endblock;
        }
        case CH_preddeclHead: 
        case CH_cnstrHead: {
            // read name
            string name = read_name(bits);
            // read ID
            ID id = read_uchar(bits);

            int is_abc_template = bitstream_read_bit(bits);
            int has_resp = bitstream_read_bit(bits);
            
            laure_expression_set set[1];
            set->expression = 0;
            set->next = 0;
            consultS status;
            while ((status = consult_expression(session, bits, set_appender, set)) == consult_fine) {}
            if (status == consult_fine) {
                // endblock is undefined
                printf("%swarning%s: endblock was undefined while reading expressions in header", RED_COLOR, NO_COLOR);
            } else if (status == consult_stoperr) {
                // error occured
                return consult_stoperr;
            }

            // prepare bodyargs
            laure_expression_compact_bodyargs ba[1];
            ba->body_len = laure_expression_get_count(set) - (has_resp ? 1 : 0);
            ba->has_resp = has_resp;
            ba->set = set;

            // prepare expression
            laure_expression_t predicate_expr[1];
            predicate_expr->t = (h == CH_cnstrHead) ? let_constraint : let_pred;
            predicate_expr->is_header = true;
            predicate_expr->docstring = 0;
            predicate_expr->link = 0;
            predicate_expr->fullstring = 0;
            predicate_expr->flag = PREDFLAG_GET(false, is_abc_template, false);
            predicate_expr->flag2 = 0;
            predicate_expr->s = name;
            predicate_expr->ba = ba;
            
            bool is_ok = rec(predicate_expr, external_payload);
            if (! is_ok) return consult_stoperr;
            break;
        }
        default: break;
    }
    return consult_fine;
}

bool evaluator(laure_expression_t *expr, void *_) {
    assert(CONTEXT);
    qresp qr = laure_eval(CONTEXT, expr, 0);
    return true;
}


void laure_compiler_consult_bytecode(laure_session_t *session, Bitstream *bits) {

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

    debug("creating context");
    CONTEXT = control_new(session, session->scope, 0, 0, 0, true);

    // CONSULTING EXPRESSIONS
    while (consult_expression(session, bits, evaluator, 0) == consult_fine) {}
}