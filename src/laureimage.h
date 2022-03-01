#ifndef IMAGE_H
#define IMAGE_H

#include "laurelang.h"
#include "utils.h"
#include "bigint.h"
#include "predpub.h"
#include "domain.h"

// U (0) - not instantiated
// I (1) - instantiated
// M (2) - multiplicity
enum ImageState {
    U, // Uninstantiated, not unified
    I, // Instantiated, unified
    M, // instantiated Multiplicity
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
    CHOICE,
    IMG_CUSTOM_T,
};

typedef struct {
    gen_resp (*rec)(void*, void*);
    void* external_ctx;
    laure_stack_t *stack;
    int length;
    struct ArrayImage *aid;
    struct ArrayImage *im;
} GenArrayCtx;

typedef struct {
    gen_resp (*rec)(void*, void*);
    void* external_ctx;
    laure_stack_t *stack;
    void *im;
    void *im2;
    GenArrayCtx *gac;
} GenCtx;

typedef struct {
    int8_t amount;
    int8_t capacity;
    void **members;
} multiplicity;

#define MULTIPLICITY_CAPACITY 8

multiplicity *multiplicity_create();
multiplicity *multiplicity_deepcopy(laure_stack_t*, multiplicity*);
gen_resp multiplicity_generate(multiplicity *mult, gen_resp (*rec)(void*, void*), void* context);
void multiplicity_insert(multiplicity*, void *img);
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

enum IntImageT {
    BINT,
    MULT,
};

// `int` image
// size should be 32 bytes
struct IntImage {
    IMAGE_HEAD
    enum ImageState state;
    enum IntImageT datatype;
    union {
        bigint* i_data;
        Domain *u_data;
        multiplicity *mult;
    };
};

struct CharImage {
    IMAGE_HEAD;
    bool is_set;
    int c;
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

// instantiated array data
struct ArrayIData {
    int length;
    Instance** array;
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
    union {
        struct ArrayIData i_data;
        struct ArrayUData u_data;
        multiplicity *mult;
    };
};

typedef struct ChoiceImage {
    IMAGE_HEAD
    Instance **multiplicity;
    int length;
} choice_img;


// abstract image
struct AtomImage {
    IMAGE_HEAD
    bool unified;
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
    laure_stack_t        *stack;
} preddata;

void *pd_get_arg(preddata *pd, int index);

// work with control when generating
typedef struct ControlCtx {
    laure_stack_t*  stack;
    qcontext*       qctx;
    var_process_kit* vpk;
    void*           data;
    bool          silent;
} control_ctx;

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
    struct InstanceSet *args;
    uint *nestings;
    Instance* resp;
    uint response_nesting;
};


struct PredicateImage {
    IMAGE_HEAD
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
    int argc;
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
} ImageHead;

typedef struct EnhancedImageHead {
    ENHANCED_IMAGE_HEAD(void);
} EnhancedImageHead;

// --- methods ---

// reading image head

ImageHead read_head(void *img);
EnhancedImageHead read_enhanced_head(void *img);

// Create image
void *integer_i_new(int data);
void *integer_u_new();
void *array_i_new(Instance **array, int length);
void *array_u_new(Instance *element);
void *abstract_new(string atom);
struct PredicateImage *predicate_header_new(struct InstanceSet *args, Instance *resp, bool is_constraint);

void *image_deepcopy(laure_stack_t *stack, void *img);

// Modify image
// * add bodied variation to predicate image
void predicate_addvar(
    void *img, 
    laure_expression_t *exp
);

// predicate variation set (pvs)
struct PredicateImageVariationSet *pvs_new();
void pvs_push(struct PredicateImageVariationSet*, struct PredicateImageVariation);

choice_img *choice_img_new();

void choice_img_add(choice_img *cimg, Instance *n);

// create predfinal (final predicate wrapper)
predfinal *get_pred_final(struct PredicateImageVariation);

// used to generate instantiated images
// [0] Image*: not instantiated image to generate from
// [1] void (*rec) (Image*, void*): receiver function, takes instantiated image and context
// [2] void*: context to pass into receiver
gen_resp image_generate(laure_stack_t*, void*, gen_resp (*rec)(void*, void*), void*);


// miscellanous

struct ArrayIData convert_string(string str, laure_stack_t* stack);

// translators

bool int_translator(laure_expression_t*, void*, laure_stack_t*);
bool char_translator(laure_expression_t*, void*, laure_stack_t*);
bool array_translator(laure_expression_t*, void*, laure_stack_t*);

// translator (a tool to work with macro_string to image conversions)

struct Translator {
    // needed to cast macro string to image
    bool (*invoke)(laure_expression_t*, void*, laure_stack_t*); // (exp, image, stack)
};

struct Translator *new_translator(bool (*invoke)(string, void*, laure_stack_t*));

struct predicate_arg {
    int index;
    void* arg;
    bool is_instance;
};

struct InstanceSet {
    Instance **data;
    int len;
};

// hints

struct PredicateCImageHint *hint_new(string, Instance*);
void laure_ensure_bigint(struct IntImage* img);

size_t image_get_size_deep(void *image);
string array_repr(Instance *ins);
string char_repr(Instance *ins);
string string_repr(Instance *ins);

// --- methods ---

// Create instance
Instance *instance_new(string name, string doc, void *image);
Instance *instance_deepcopy(laure_stack_t*, string name, Instance *from_instance);
Instance *instance_shallow_copy(string name, Instance *from_instance);
Instance *instance_deepcopy_with_image(laure_stack_t*, string name, Instance *from_instance, void *image);
size_t instance_get_size_deep(Instance *instance);
string instance_repr(Instance*);
string instance_get_doc(Instance *ins);
void instance_lock(Instance *ins);
void instance_unlock(Instance *ins);

Instance *choice_instance_new(string name, string doc);

// instance set
struct InstanceSet *instance_set_new();
void instance_set_push(struct InstanceSet*, Instance*);

// work with preddata (predicate call data)
preddata *preddata_new(laure_stack_t *stack);
void preddata_push(preddata*, struct predicate_arg);
struct predicate_arg *predicate_arg_set_new();

control_ctx *create_control_ctx(laure_stack_t* stack, qcontext* qctx, var_process_kit* vpk, void* data);
qresp        image_control     (void *inst_img, control_ctx* ctx);

// used to determine if instance is instantiated or not
bool instantiated(Instance*);
bool instantiated_or_mult(Instance*);

// translators

bool int_translator(laure_expression_t*, void*, laure_stack_t *stack);
bool char_translator(laure_expression_t*, void*, laure_stack_t *stack);
bool array_translator(laure_expression_t*, void*, laure_stack_t *stack);
bool string_translator(laure_expression_t*, void*, laure_stack_t *stack);
bool atom_translator(laure_expression_t*, void*, laure_stack_t *stack);

multiplicity *translate_to_multiplicity(
    laure_expression_t*, 
    bool (*transl) (laure_expression_t*, void*, laure_stack_t*, bool),
    void *rimg,
    laure_stack_t *stack
);

// --- miscellaneous ---
string predicate_repr(Instance*);
string constraint_repr(Instance*);
string atom_repr(Instance*);
bool img_equals(void*, void*);
char convert_escaped_char(char c);

// checks for predicates
bool is_array_of_int(Instance*);
bool is_int(Instance*);

bool int_check(void *img_, void *bi_);

#endif
