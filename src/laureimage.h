#ifndef LAUREIMAGE_H
#define LAUREIMAGE_H

#include "laurelang.h"
#include "bigint.h"
#include "domain.h"

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
} laure_typedecl_t;

typedef struct laure_typedecl {
    laure_typedecl_t t;
    union {
        Instance *instance;
        string generic;
    };
} laure_typedecl;

typedef struct laure_typeset {
    uint length;
    laure_typedecl *data;
} laure_typeset;

laure_typeset *laure_typeset_new();
void laure_typeset_push_instance(laure_typeset *ts, Instance *instance);
void laure_typeset_push_decl(laure_typeset *ts, string generic_name);
bool laure_typeset_all_instances(laure_typeset *ts);

laure_typedecl *laure_typedecl_instance_create(Instance *instance);
laure_typedecl *laure_typedecl_generic_create(string generic_name);

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
    bool       fully_instantiated;
    int        len;
    Instance **data;
} laure_structure_t;

typedef struct StructureImage {
    IMAGE_HEAD
    bool is_initted;
    union {
        laure_expression_t        *header;
        laure_structure_t          structure;
    };
} laure_structure;

typedef struct array_linked {
    Instance *data;
    struct array_linked *next;
} array_linked_t;

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
    long length_lid;
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

struct StructureElement {
    string key;
    Instance* value;
    int link_id;
};

 //! todo structure image
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
long pd_get_arg_link(preddata *pd, int index);

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
    bool          silent, no_ambig;
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
};


struct PredicateImage {
    IMAGE_HEAD
    bool is_primitive;
    struct PredicateHeaderImage header;
    struct PredicateImageVariationSet *variations;
};

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

struct PredicateFinalInterior {
    string* argn;
    int     argc;
    string respn;
    laure_expression_set *body;
};

typedef struct PredicateFinal {
    enum PredicateFinalT t;
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

struct PredicateImage *predicate_header_new(laure_typeset *args, laure_typedecl *resp, bool is_constraint);

void *image_deepcopy(laure_scope_t *scope, void *img);

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
predfinal *get_pred_final(struct PredicateImageVariation);

// used to generate instantiated images
// [0] Image*: not instantiated image to generate from
// [1] void (*rec) (Image*, void*): receiver function, takes instantiated image and context
// [2] void*: context to pass into receiver
gen_resp image_generate(laure_scope_t*, void*, gen_resp (*rec)(void*, void*), void*);


// miscellanous

struct ArrayIData convert_string(string str, laure_scope_t* scope);

// translators

bool int_translator(laure_expression_t*, void*, laure_scope_t*);
bool char_translator(laure_expression_t*, void*, laure_scope_t*);
bool array_translator(laure_expression_t*, void*, laure_scope_t*);
bool atom_translator(laure_expression_t*, void*, laure_scope_t*);

// translator (a tool to work with macro_string to image conversions)

struct Translator {
    char identificator;
    // needed to cast macro string to image
    bool (*invoke)(laure_expression_t*, void*, laure_scope_t*); // (exp, image, scope)
};

struct Translator *new_translator(char identificator, bool (*invoke)(string, void*, laure_scope_t*));

struct predicate_arg {
    int index;
    void* arg;
    bool is_instance;
    long link_id;
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
Instance *instance_deepcopy(laure_scope_t*, string name, Instance *from_instance);
Instance *instance_shallow_copy(Instance *from_instance);
Instance *instance_deepcopy_with_image(laure_scope_t*, string name, Instance *from_instance, void *image);
size_t instance_get_size_deep(Instance *instance);
string instance_repr(Instance*);
string instance_get_doc(Instance *ins);
void instance_lock(Instance *ins);
void instance_unlock(Instance *ins);

// instance set
struct InstanceSet *instance_set_new();
void instance_set_push(struct InstanceSet*, Instance*);

// work with preddata (predicate call data)
preddata *preddata_new(laure_scope_t *scope);
void preddata_push(preddata*, struct predicate_arg);
struct predicate_arg *predicate_arg_set_new();

control_ctx *create_control_ctx(laure_scope_t* scope, qcontext* qctx, var_process_kit* vpk, void* data, bool no_ambig);
qresp        image_control     (void *inst_img, control_ctx* ctx);

// used to determine if instance is instantiated or not
bool instantiated(Instance*);

// translators

bool int_translator(laure_expression_t*, void*, laure_scope_t *scope);
bool char_translator(laure_expression_t*, void*, laure_scope_t *scope);
bool array_translator(laure_expression_t*, void*, laure_scope_t *scope);
bool string_translator(laure_expression_t*, void*, laure_scope_t *scope);
bool atom_translator(laure_expression_t*, void*, laure_scope_t *scope);

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
// --

bool image_equals(void*, void*);
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

/* API */

Instance *laure_api_add_predicate(
    laure_session_t *session,
    string name,
    qresp (*callable)(preddata*, control_ctx*),
    uint argc, string arg_hints, string response_hint,
    bool is_constraint, string doc
);

#endif