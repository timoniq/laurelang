#ifndef LAUREIMAGE_H
#define LAUREIMAGE_H

#include "laurelang.h"
#include "bigint.h"
#include "domain.h"
#include <uuid/uuid.h>

#define MAX_ARGS 32
#define GENERIC_PLACES 26
#define GENERIC_FIRST 'A'
#define GENERIC_LAST 'Z'

// U (0) - not instantiated
// I (1) - instantiated
enum ImageState {
    U,
    I
};

// Image types
enum ImageT {
    INTEGER,
    CHAR,
    ARRAY,
    ATOM,
    PREDICATE_FACT,
    CONSTRAINT_FACT,
    STRUCTURE,
    IMG_CUSTOM_T,
    UNION,
    UUID,
    FORMATTING,
    LINKED
};

typedef struct {
    uint idx;
    long link_id;
} ref_element;

typedef struct {
    gen_resp (*rec)(void*, void*);
    void* external_ctx;

    laure_scope_t *scope;
    struct ArrayImage *aid, *im;

    uint length;
} GenArrayCtx;

typedef struct {
    gen_resp (*rec)(void*, void*);
    void* external_ctx;
    laure_scope_t *scope;
    void *im;
    void *im2;
    GenArrayCtx *gac;
} GenCtx;

typedef struct {
    int8_t amount;
    int8_t capacity;
    void **members;
} multiplicity;

typedef enum laure_typedecl_t {
    td_instance,
    td_generic,
    td_auto
} laure_typedecl_t;

typedef struct laure_typedecl {
    laure_typedecl_t t;
    union {
        Instance *instance;
        int generic;
        laure_auto_type auto_type;
    };
} laure_typedecl;

typedef struct laure_typeset {
    uint length;
    laure_typedecl *data;
} laure_typeset;

laure_typeset *laure_typeset_new();
void laure_typeset_push_instance(laure_typeset *ts, Instance *instance);
void laure_typeset_push_decl(laure_typeset *ts, string generic_name);
void laure_typeset_push_auto(laure_typeset *ts, laure_auto_type auto_type);

bool laure_typeset_all_instances(laure_typeset *ts);

laure_typedecl *laure_typedecl_instance_create(Instance *instance);
laure_typedecl *laure_typedecl_generic_create(string generic_name);
laure_typedecl *laure_typedecl_auto_create(laure_auto_type auto_type);

#define MULTIPLICITY_CAPACITY 8

multiplicity *multiplicity_create();
multiplicity *multiplicity_deepcopy(multiplicity*);
void multiplicity_insert(multiplicity*, void *ptr);
void multiplicity_free(multiplicity*);

#define IMAGE_HEAD \
    bool mark; \
    enum ImageT t; \
    struct Translator* translator;

#define ENHANCED_IMAGE_HEAD(self) \
    IMAGE_HEAD \
    char *identifier; \
    bool (*eq)(self*, self*); \
    bool (*is_instantiated)(self*); \
    self* (*copy)(self*);

// Predicate types
// rel - relation predicate
// header - header predicate
// bodied - predicate with body
// c - predicate with c source
enum PredicateT {
    PREDICATE_NORMAL,
    PREDICATE_HEADER,
    PREDICATE_C
};

// `int` image
// size should be 32 bytes
struct IntImage {
    IMAGE_HEAD
    enum ImageState state;
    union {
        bigint* i_data;
        Domain *u_data;
    };
};

enum LinkedT {
    LINKED_STRUCTURE_FIELD
};

typedef struct laure_linked_image {
    IMAGE_HEAD
    enum LinkedT linked_type;
    ulong link;
    union {
        size_t idx;
    };
} laure_linked_image;


Instance *linked_create_instance_structure_field(string name, ulong structure_link, size_t i);

struct CharImage {
    IMAGE_HEAD;
    // states:
    // 0 - unset
    // 1 - one
    // 2 - charset
    unsigned short state;
    union {
        int c;
        string charset;
    };
};

typedef struct {
    bool is_construct;
    string name;
    union {
        Instance *instance;
        laure_expression_t *construct;
    };
} laure_structure_element;

typedef struct {
    bool       fully_instantiated;
    int        count;
    laure_structure_element *data;
} laure_structure_data;

typedef struct StructureImage {
    IMAGE_HEAD
    bool is_initted;
    union {
        laure_expression_t        *header;
        laure_structure_data       data;
    };
} laure_structure;

laure_structure *laure_structure_create_header(laure_expression_t *e);
qresp structure_init(laure_structure *structure, laure_scope_t *scope);
laure_structure *structure_new_image(laure_structure *img, laure_scope_t *scope);

typedef struct UnionImage {
    struct InstanceSet *united_set;
} laure_union_image;

typedef struct array_linked {
    Instance *data;
    struct array_linked *next;
} array_linked_t;

typedef struct Pocket {
    ulong variable;
    size_t size;
    array_linked_t *first, *last;

    struct Pocket *next;
} Pocket;

typedef struct Bag {
    Pocket *linked_pocket;
} Bag;

typedef struct laure_qcontext {
    laure_expression_set *expset;
    struct laure_qcontext *next;
    bool constraint_mode, flagme;
    Bag *bag;
} qcontext;

void ensure_bag(qcontext *qctx);
Pocket *pocket_get_or_new(Bag *bag, unsigned long variable);

// instantiated array data
struct ArrayIData {
    uint length;
    array_linked_t *linked;
};

// not instantiated array data
struct ArrayUData {
    Domain *length;
};

// `T[]` image
// T - .arr_el
struct ArrayImage {
    IMAGE_HEAD
    enum ImageState state;
    Instance *arr_el;
    unsigned long length_lid;
    uint ref_count;
    ref_element *ref;
    union {
        struct ArrayIData i_data;
        struct ArrayUData u_data;
        multiplicity *mult;
    };
};


// abstract image
struct AtomImage {
    IMAGE_HEAD
    bool single;
    union {
        string atom;
        multiplicity *mult;
    };
};

struct FormattingPart {
    string before;
    string name;
    struct FormattingPart *prev, *next;
};

struct FormattingImage {
    IMAGE_HEAD
    struct FormattingPart *last, *first;
};

struct FormattingImage *laure_create_formatting_image(struct FormattingPart *linked);
Instance *linked_resolve(laure_linked_image *im, laure_scope_t *scope);
struct FormattingPart *laure_parse_formatting(string fmt);
string formatting_repr(Instance *instance);
int formatting_to_pattern(
    struct FormattingPart *first, 
    pattern_element **elements,
    size_t sz,
    size_t *count
);

struct StructureElement {
    string key;
    Instance* value;
    int link_id;
};

// fully_instantiated - structure is instantiated
// next_uid - next uid for uname generation

typedef struct {
    int                   argc;
    struct predicate_arg *argv;
    void*                 resp;
    long                  resp_link;
    laure_scope_t        *scope;
} preddata;

void *pd_get_arg(preddata *pd, int index);
unsigned long pd_get_arg_link(preddata *pd, int index);

void preddata_free(preddata *pd);

// storing links to grab when merging scopes
typedef struct laure_grab_linked {
    ulong link;
    struct laure_grab_linked *next;
} laure_grab_linked;

// work with control when generating
typedef struct laure_control_ctx {
    laure_session_t *session;
    laure_scope_t  *scope, *tmp_answer_scope;
    qcontext*       qctx;
    var_process_kit* vpk;
    void*           data;
    bool          silent, no_ambig, this_break;
    ulong cut; // scope id to cut up to
#ifndef DISABLE_WS
    laure_ws *ws;
#endif
} control_ctx;

void laure_add_grabbed_link(control_ctx *cctx, ulong nlink);
void laure_free_grab(laure_grab_linked *grab);

#define grab_linked_iter(cctx, func, ...) do {\
            laure_grab_linked *__l = cctx->grabbed; \
            while (__l) { \
                func(__l->link, __VA_ARGS__); \
                __l = __l->next; \
            } \
        } while (0)

struct PredicateCImageHint {
    string name;
    Instance *hint;
};

struct PredicateCImage {
    qresp (*pred)(preddata*, control_ctx*);
    int argc;
    struct PredicateCImageHint** hints;
    Instance* resp_hint;
};

struct PredicateImageVariation {
    enum PredicateT t;
    union {
        laure_expression_t         *exp;
        struct PredicateCImage      c;
    };
};

typedef struct PredicateFinal predfinal;

struct PredicateImageVariationSet {
    int len;
    struct PredicateImageVariation* set;
    predfinal **finals;
};

struct PredicateHeaderImage {
    laure_typeset *args;
    uint *nestings;
    laure_typedecl* resp;
    uint response_nesting;
    bool do_ordering;
};

typedef struct predicate_bound_types_result {
    int code;
    union {
        struct PredicateImage *bound_predicate;
        string err;
    };
} predicate_bound_types_result;

predicate_bound_types_result pbtr_error(int code, string err);
predicate_bound_types_result pbtr_ok(struct PredicateImage *image);

// creates bound version of predicate
predicate_bound_types_result laure_dom_predicate_bound_types(
    laure_scope_t *scope,
    struct PredicateImage *predicate_im_unbound,
    laure_expression_set *clarifiers
);

struct PredicateImage {
    IMAGE_HEAD
    bool is_primitive;
    string bound;
    struct PredicateHeaderImage header;
    struct PredicateImageVariationSet *variations;
};

typedef struct UUIDImage {
    IMAGE_HEAD
    string bound;
    uuid_t uuid;
    bool unset;
} laure_uuid_image;

void force_predicate_to_uuid(struct PredicateImage *predicate_img);

struct ConstraintImage {
    IMAGE_HEAD
    struct PredicateHeaderImage header;
    struct PredicateImageVariationSet *variations;
};

// --- final ---

enum PredicateFinalT {
    PF_INTERIOR,
    PF_C
};

struct PredicateFinalC {
    qresp (*pred)(preddata*, control_ctx*);
    int argc; // -1 is any
    struct PredicateCImageHint** hints;
    Instance* resp_hint;
};

typedef struct bintree_permut {
    laure_expression_set *set; // is set if terminal
    struct bintree_permut *_0;
    struct bintree_permut *_1;
} bintree_permut;

typedef struct predicate_linked_permutations {
    bool fixed;
    union {
        bintree_permut *tree;
        laure_expression_set *fixed_set;
    };
} predicate_linked_permutations;

predicate_linked_permutations laure_generate_final_permututations(
    laure_expression_set *unlinked,
    size_t argc,
    bool has_resp
);

predicate_linked_permutations laure_generate_final_fixed(
    laure_expression_set *set
);

laure_expression_set *laure_get_ordered_predicate_body(
    predicate_linked_permutations plp, 
    size_t argc,
    bool *argi,
    bool resi
);

struct PredicateFinalInterior {
    string* argn;
    int     argc;
    bool resp_auto;
    union {
        string respn;
        laure_auto_type auto_type;
    };
    predicate_linked_permutations plp;
    laure_expression_set *body;
};

typedef struct PredicateFinal {
    enum PredicateFinalT t;
    uuid_t uu;
    union {
        struct PredicateFinalInterior interior;
        struct PredicateFinalC c;
    };
} predfinal;

typedef struct ImageHead {
    IMAGE_HEAD
} laure_image_head;

typedef struct EnhancedImageHead {
    ENHANCED_IMAGE_HEAD(void);
} laure_image_head_enh;

// --- methods ---

// reading image head

laure_image_head read_head(void *img);
laure_image_head_enh read_enhanced_head(void *img);

// Create image
struct IntImage *laure_create_integer_i(int value);
struct IntImage *laure_create_integer_u(Domain *dom);

struct CharImage *laure_create_char_u();
struct CharImage *laure_create_char_i(int c);
struct CharImage *laure_create_charset(string charset);

struct ArrayImage *laure_create_array_u(Instance *el_t);

struct AtomImage *laure_atom_universum_create(multiplicity *mult);

struct PredicateImage *predicate_header_new(string bound_name, laure_typeset *args, laure_typedecl *resp, bool is_constraint);

laure_uuid_image *laure_create_uuid(string bound, uuid_t uu);
Instance *laure_create_uuid_instance(string name, string bound, string uu_str);

void *image_deepcopy(void *img);

// Modify image
// * add bodied variation to predicate image
void predicate_addvar(
    void *img, 
    laure_expression_t *exp
);

// predicate variation set (pvs)
struct PredicateImageVariationSet *pvs_new();
void pvs_push(struct PredicateImageVariationSet*, struct PredicateImageVariation);

// create predfinal (final predicate wrapper)
predfinal *get_pred_final(struct PredicateImage*, struct PredicateImageVariation);

// used to generate instantiated images
// [0] Image*: not instantiated image to generate from
// [1] void (*rec) (Image*, void*): receiver function, takes instantiated image and context
// [2] void*: context to pass into receiver
gen_resp image_generate(laure_scope_t*, void*, gen_resp (*rec)(void*, void*), void*);

// miscellanous

struct ArrayIData convert_string(string unicode_str, laure_scope_t* scope);
int convert_to_string(struct ArrayIData i_data, string buff);

// translators

bool int_translator(laure_expression_t*, void*, laure_scope_t*, ulong);
bool char_translator(laure_expression_t*, void*, laure_scope_t*, ulong);
bool array_translator(laure_expression_t*, void*, laure_scope_t*, ulong);
bool atom_translator(laure_expression_t*, void*, laure_scope_t*, ulong);
bool structure_translator(laure_expression_t*, void*, laure_scope_t*, ulong);

// translator (a tool to work with macro_string to image conversions)

struct Translator {
    char identificator;
    // needed to cast macro string to image
    bool (*invoke)(laure_expression_t*, void*, laure_scope_t*, ulong); // (exp, image, scope, link)
};

struct Translator *new_translator(char identificator, bool (*invoke)(string, void*, laure_scope_t*, ulong));

struct predicate_arg {
    int index;
    void* arg;
    bool is_instance;
    unsigned long link_id;
};

struct InstanceSet {
    Instance **data;
    int len;
};

// hints

struct PredicateCImageHint *hint_new(string, Instance*);
void laure_ensure_bigint(struct IntImage* img);

size_t image_get_size_deep(void *image);

// --- methods ---

// Create instance
Instance *instance_new(string name, string doc, void *image);
Instance *instance_deepcopy(string name, Instance *from_instance);
Instance *instance_shallow_copy(Instance *from_instance);
Instance *instance_deepcopy_with_image(laure_scope_t*, string name, Instance *from_instance, void *image);
Instance *instance_new_copy(string name, Instance *instance, laure_scope_t *scope);
size_t instance_get_size_deep(Instance *instance);
string instance_repr(Instance *instance, laure_scope_t *scope);
string instance_get_doc(Instance *ins);
void instance_lock(Instance *ins);
void instance_unlock(Instance *ins);

bool laure_safe_update(Instance *ninstance, Instance *oinstance, laure_scope_t *nscope, laure_scope_t *oscope);

// instance set
struct InstanceSet *instance_set_new();
void instance_set_push(struct InstanceSet*, Instance*);

// work with preddata (predicate call data)
preddata *preddata_new(laure_scope_t *scope);
void preddata_push(preddata*, struct predicate_arg);
struct predicate_arg *predicate_arg_set_new();

control_ctx *create_control_ctx(laure_scope_t* scope, qcontext* qctx, var_process_kit* vpk, void* data, bool no_ambig);
qresp        image_control     (void *inst_img, control_ctx* ctx);

// used to determine if image/instance is instantiated or not
bool image_instantiated(void *image);
bool instantiated(Instance*);

// translators

bool int_translator(laure_expression_t*, void*, laure_scope_t *scope, ulong);
bool char_translator(laure_expression_t*, void*, laure_scope_t *scope, ulong);
bool array_translator(laure_expression_t*, void*, laure_scope_t *scope, ulong);
bool string_translator(laure_expression_t*, void*, laure_scope_t *scope, ulong);
bool atom_translator(laure_expression_t*, void*, laure_scope_t *scope, ulong);

multiplicity *translate_to_multiplicity(
    laure_expression_t*, 
    bool (*transl) (laure_expression_t*, void*, laure_scope_t*, bool),
    void *rimg,
    laure_scope_t *scope
);

// --- miscellaneous ---

// -- repr --
string int_repr(Instance *ins);
string array_repr(Instance *ins);
string char_repr(Instance *ins);
string string_repr(Instance *ins);
string predicate_repr(Instance*);
string constraint_repr(Instance*);
string atom_repr(Instance*);
string uuid_repr(Instance*);
string structure_repr(Instance*);
string structure_repr_detailed(Instance*, laure_scope_t*);
// --

bool image_equals(void*, void*, laure_scope_t*);
void image_free(void*);
char convert_escaped_char(char c);

#define ATOM_LEN_MAX 64

void write_atom_name(string r, char *write_to);

// checks for predicates
bool is_array_of_int(Instance*);
bool is_int(Instance*);

bool int_check(void *img_, bigint *bi);

int laure_convert_esc_ch(int c, char *write);
int laure_convert_ch_esc(int c);

Instance *get_nested_instance(Instance *atom, uint nesting, laure_scope_t *scope);
Instance *laure_unwrap_nestings(Instance *wrapped, uint redundant_nestings);
string get_final_name(predfinal *pf, string shorthand);
string get_default_name(predfinal *pf, string final_name);

/* API */

Instance *laure_api_add_predicate(
    laure_session_t *session,
    string name,
    qresp (*callable)(preddata*, control_ctx*),
    uint argc, string arg_hints, string response_hint,
    bool is_constraint, string doc
);

#endif