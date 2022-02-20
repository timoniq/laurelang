#ifndef DYN_DOM_H
#define DYN_DOM_H

#include "laurelang.h"
#include "bigint.h"
#include "predpub.h"

enum Value {
    INFINITE,
    INCLUDED,
    SECLUDED
};

typedef struct IntValue {
    enum Value t;
    bigint *data;
} IntValue;

enum DomainT {
    DOMAIN,
    SINGLE,
    EMPTY
};

typedef struct Domain {
    enum DomainT t;

    IntValue lborder;
    IntValue rborder;
    void *constraints;
    int constraints_len;

} Domain;

void int_convert_to_secluded(IntValue *v, bool gt);

Domain  *int_domain_new       ();
void     int_domain_gt        (Domain *dom, IntValue v);
void     int_domain_lt        (Domain *dom, IntValue v);
bool     int_domain_check(Domain *dom, bigint* i);
void     int_domain_constraint(Domain *dom, IntValue v);
gen_resp int_domain_generate  (Domain *dom, gen_resp (*receiver)(bigint*, void*), void* context);
char    *int_domain_repr      (Domain *dom);
Domain  *int_domain_copy      (Domain *dom);
void     int_domain_free      (Domain *dom);
Domain  *int_domain_single    (bigint *p);

#endif