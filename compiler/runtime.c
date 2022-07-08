// laurelang compiler
// (c) Arseny Kriuchkov, 2022

#include "compiler.h"

control_ctx *CONTEXT = 0;
uint LAURE_COMPILER_ID_OFFSET = 0;
uint END_WITH_OFFSET = 0;

bool HAS_RESP[ID_MAX];
uint VAR_LINKS_BY_ID[ID_MAX];

bool NBID_INITTED = false;
string NAME_BY_ID[ID_MAX];

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

consultS consult_expression(
    laure_session_t *session, 
    Bitstream *bits,
    CONSULT_EXP_REC(rec),
    void *external_payload
);

bool set_appender(laure_expression_t *expr, laure_expression_set *set) {
    while (set->next) {
        set = set->next;
    }
    set->expression = expr;
    set->next = 0;
    return true;
}

bool set_expr(laure_expression_t *exp, laure_expression_t *ptr) {
    *ptr = *exp;
    return true;
}

void print_bits(bool bits[], uint n) {
    printf("[");
    for (uint i = 0; i < n; i++) printf("%d", bits[i]);
    printf("]\n");
}

unsigned char read_uchar(Bitstream *bits) {
    bool byte[8];
    bool can_read = read_bits(bits, 8, byte);
    if (! can_read) return 0;
    unsigned char c = (unsigned char)(bitset_toint(byte, 8));
    return c;
}

ID read_ID(Bitstream *bits) {
    ID id = read_uchar(bits);
    if (id >= END_WITH_OFFSET)
        END_WITH_OFFSET = id + 1;
    id += LAURE_COMPILER_ID_OFFSET;
    assert(id < ID_MAX);
    return id;
}


consultS read_until_endblock(laure_session_t *session, Bitstream *bits, laure_expression_set *set) {
    consultS status;
    uint i = 0;
    while (true) {
        laure_expression_t expr[1];
        status = consult_expression(session, bits, set_expr, expr);
        if (status != consult_fine) {
            return status;
        }
        if (i != 0) {
            laure_expression_set new_set[1];
            new_set->expression = 0;
            new_set->next = 0;

            set->next = new_set;
            set = set->next;
        }
        set->expression = expr;
        i++;
    }
    return consult_fine;
}


string read_name(Bitstream *bits) {
    bool length_byte[8];
    bool can_read = read_bits(bits, CHAR_BIT, length_byte);
    if (! can_read) return 0;
    unsigned char length = bitset_uchar(length_byte);
    string s = malloc(length + 1);
    s[length] = 0;
    for (unsigned char i = 0; i < length; i++) {
        bool cbits[8];
        read_bits(bits, 8, cbits);
        unsigned char c = (unsigned char)(bitset_toint(cbits, 8));
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
    bool header[HEADER_BITS];
    bool can_read = read_bits(bits, HEADER_BITS, header);
    if (! can_read) return consult_stoperr;
    int h = bitset_toint(header, HEADER_BITS);

    laure_expression_t expr[1];
    expr->flag = 0;
    expr->docstring = 0;
    expr->fullstring = 0;
    expr->is_header = 0;
    expr->link = 0;
    expr->s = 0;
    expr->ba = 0;
    expr->flag2 = 0;
    
    switch (h) {
        case CH_endblock: {
            return consult_endblock;
        }
        case CH_var: 
        case CH_nestedvar:
        case CH_unify: {
            if (h == CH_var || h == CH_nestedvar)
                expr->t = let_var;
            else expr->t = let_unify;

            ID id = read_ID(bits);
            expr->flag2 = id;
            if (h == CH_nestedvar) {
                bool nesting_bits[COUNT_BITS_NESTING];
                bool can_read = read_bits(bits, COUNT_BITS_NESTING, nesting_bits);
                if (! can_read) {
                    printf("panic: can't read nesting\n");
                    return consult_stoperr;
                }
                int nesting = bitset_toint(nesting_bits, COUNT_BITS_NESTING);
                expr->flag = nesting;
            }
            break;
        }
        case CH_cut: {
            expr->t = let_cut;
            break;
        }
        case CH_call: {
            ID id = read_ID(bits);
            laure_expression_set set[1];
            memset(set, 0, sizeof(laure_expression_set));

            consultS status = read_until_endblock(session, bits, set);
            if (status == consult_stoperr) return status;

            laure_expression_compact_bodyargs ba[1];
            ba->has_resp = false;
            ba->body_len = laure_expression_get_count(set);
            ba->set = set;

            expr->t = let_pred_call;
            expr->ba = ba;
            
            if (HEAP_TABLE[id])
                expr->flag2 = VAR_LINK_LIMIT + id;
            else {
                if (! NAME_BY_ID[id]) {
                    printf("not found predicate call name (probably missing CH_give_name instruction)\n");
                    return consult_stoperr;
                }
                expr->s = NAME_BY_ID[id];
            }
            break;
        }
        case CH_give_id: {
            ID id = read_ID(bits);
            string name = read_name(bits);
            NAME_BY_ID[id] = name;
            return consult_fine;
        }
        case CH_isol_start: {
            laure_expression_set set[1];
            set->expression = 0;
            set->next = 0;

            consultS status;
            while ((status = consult_expression(session, bits, set_appender, set)) == consult_fine) {
            }
            if (status == consult_fine) {
                printf("%swarning%s: endblock was undefined while reading expressions in isolated set", RED_COLOR, NO_COLOR);
            } else if (status == consult_stoperr) {
                return consult_stoperr;
            }

            laure_expression_compact_bodyargs ba[1];
            ba->body_len = laure_expression_get_count(set);
            ba->has_resp = 0;
            ba->set = set;

            expr->t = let_set;
            expr->flag = 1;
            expr->ba = ba;
            break;
        }
        case CH_assertV2V:
        case CH_img: {
            if (h == CH_assertV2V) expr->t = let_assert;
            else expr->t = let_image;

            ID lid = read_ID(bits);
            ID rid = read_ID(bits);
            
            laure_expression_t left[1];
            laure_expression_t right[1];
            memset(left, 0, sizeof(laure_expression_t));
            memset(right, 0, sizeof(laure_expression_t));

            left->t = let_var;
            right->t = let_var;
            left->flag2 = lid;
            right->flag2 = rid;

            laure_expression_set set2[1];
            set2->expression = right;
            set2->next = 0;
            
            laure_expression_set set[1];
            set->expression = left;
            set->next = set2;

            laure_expression_compact_bodyargs ba[1];
            ba->body_len = laure_expression_get_count(set);
            ba->has_resp = 0;
            ba->set = set;
            
            expr->ba = ba;
            break;
        }
        case CH_assertV2VAL: {
            ID id = read_ID(bits);
            string s = read_name(bits);

            laure_expression_t var[1];
            memset(var, 0, sizeof(laure_expression_t));
            var->t = let_var;
            var->flag2 = id;

            laure_expression_t value[1];
            memset(value, 0, sizeof(laure_expression_t));
            value->t = let_custom;
            value->s = s;

            laure_expression_set set2[1];
            set2->expression = var;
            set2->next = 0;
            
            laure_expression_set set[1];
            set->expression = value;
            set->next = set2;
            
            laure_expression_compact_bodyargs ba[1];
            ba->body_len = laure_expression_get_count(set);
            ba->has_resp = 0;
            ba->set = set;

            expr->t = let_assert;
            expr->ba = ba;
            break;
        }
        case CH_data: {
            string value = read_name(bits);
            expr->t = let_custom;
            expr->s = value;
            break;
        }
        case CH_preddeclHead: 
        case CH_cnstrHead: {
            // read name
            string name = read_name(bits);
            // read ID
            ID id = read_ID(bits);

            int is_abc_template = bitstream_read_bit(bits);
            int has_resp = bitstream_read_bit(bits);

            HAS_RESP[id] = (bool)has_resp;
            
            laure_expression_set set[1];
            set->expression = 0;
            set->next = 0;

            consultS status = read_until_endblock(session, bits, set);
            if (status == consult_fine) {
                // endblock is undefined
                printf("%swarning%s: endblock was undefined while reading expressions in header", RED_COLOR, NO_COLOR);
            } else if (status == consult_stoperr) {
                // error occured
                return consult_stoperr;
            }

            // prepare bodyargs
            laure_expression_compact_bodyargs ba[1];
            ba->body_len = 0;
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

            Instance *predicate_instance = _TEMP_PREDCONSULT_LAST;
            laure_set_heap_value(predicate_instance, id);

            return consult_fine;
        }
        case CH_preddecl: {
            // predicate case
            ID id = read_ID(bits);

            laure_expression_set args[1];
            args->expression = 0;
            args->next = 0;
            
            consultS status = read_until_endblock(session, bits, args);
            if (status == consult_fine) {
                // endblock is undefined
                printf("%swarning%s: endblock was undefined while reading expressions in args of predicate case", RED_COLOR, NO_COLOR);
            } else if (status == consult_stoperr) {
                // error occured
                return consult_stoperr;
            }

            bool has_resp = HAS_RESP[id];

            if (HAS_RESP[id]) {
                laure_expression_t resp[1];
                consultS status = consult_expression(session, bits, set_expr, resp);
                if (status != consult_fine) return status;
                laure_expression_set last_set[1];
                *last_set = *args;
                while (last_set->next) *last_set = *last_set->next; // ?
                last_set->expression = resp;
            }

            laure_expression_set body[1];
            body->expression = 0;
            body->next = 0;

            status = read_until_endblock(session, bits, body);
            if (status != consult_fine) return status;

            // merge sets
            laure_expression_set *linked = laure_expression_set_link_branch(args, body);

            laure_expression_compact_bodyargs ba[1];
            ba->set = linked;
            ba->has_resp = has_resp;
            ba->body_len = laure_expression_get_count(body);

            expr->t = let_pred;
            expr->flag2 = id;
            expr->ba = ba;
            break;
        }
        default: break;
    }
    bool is_ok = rec(expr, external_payload);
    if (! is_ok) return consult_stoperr;
    return consult_fine;
}

bool evaluator(laure_expression_t *expr, void *_) {
    assert(CONTEXT);
    qresp qr = laure_eval(CONTEXT, expr, 0);
    return true;
}


void laure_compiler_consult_bytecode(laure_session_t *session, Bitstream *bits) {
    if (! NBID_INITTED) {
        memset(NAME_BY_ID, 0, sizeof(bool) * ID_MAX);
    }
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

    debug("ID offset is %d", LAURE_COMPILER_ID_OFFSET);
    // CONSULTING EXPRESSIONS
    consultS status;
    align(bits);
    while ((status = consult_expression(session, bits, evaluator, 0)) == consult_fine) {
        align(bits);
    }
    if (status == consult_stoperr) {
        printf("fatal: error occured\n");
    }
    debug("during bytecode interpretation %d id unique positions were taken");
    LAURE_COMPILER_ID_OFFSET += END_WITH_OFFSET;
    debug("bytecode interpretation completed");
}