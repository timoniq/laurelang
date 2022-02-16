#include "laurelang.h"
#include "domain.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>

#ifndef INFINITY_STR
#define INFINITY_STR "∞"
#endif

Domain *int_domain_new() {
    Domain *dom = malloc(sizeof(Domain));

    IntValue start;
    IntValue end;

    start.t = INFINITE;
    start.data = NULL;
    
    end.t = INFINITE;
    end.data = NULL;

    dom->t = DOMAIN;
    dom->lborder = start;
    dom->rborder = end;
    dom->constraints = NULL;
    dom->constraints_len = 0;
    return dom;
}

void int_convert_to_secluded(IntValue *v, bool gt) {
    switch (v->t) {
        case INCLUDED: {
            if (gt) {
                v->data--;
            } else {
                ;
            }
            break;
        }
        default:
            break;
    }
}

void int_domain_gt(Domain *dom, IntValue v) {
    switch (dom->t) {
        case DOMAIN: {
            IntValue l = dom->lborder;
            if (l.t == INFINITE || l.data == NULL || bigint_cmp(v.data, l.data) == 1) {
                dom->lborder = v;
            }
            break;
        }
        default:
            break;
    }
}

void int_domain_lt(Domain *dom, IntValue v) {
    switch (dom->t) {
        case DOMAIN: {
            IntValue r = dom->rborder;
            if (r.t == INFINITE || r.data == NULL || bigint_cmp(v.data, r.data) == -1) {
                dom->rborder = v;
            }
            break;
        }
        default:
            break;
    }
}

bool int_domain_check(Domain *dom, bigint* i) {
    if (dom->lborder.t == INFINITE && dom->rborder.t == INFINITE) {
        return true;
    } else {
        if (dom->lborder.t == INFINITE) {
            if (dom->rborder.t == INCLUDED) {
                int cmp = bigint_cmp(i, dom->rborder.data);
                return cmp <= 0;
            } else {
                int cmp = bigint_cmp(i, dom->rborder.data);
                return cmp == -1;
            }
        } else if (dom->rborder.t == INFINITE) {
            if (dom->lborder.t == INCLUDED) {
                int cmp = bigint_cmp(i, dom->lborder.data);
                return cmp >= 0;
            } else {
                int cmp = bigint_cmp(i, dom->lborder.data);
                return cmp == 1;
            }
        } else {
            return ((dom->lborder.t == INCLUDED) ? bigint_cmp(i, dom->lborder.data) >= 0 : bigint_cmp(i, dom->lborder.data) > 0) && ((dom->rborder.t == INCLUDED) ? bigint_cmp(i, dom->rborder.data) <= 0 : bigint_cmp(i, dom->rborder.data) < 0);
        }
    }
}

void int_domain_constraint(Domain *dom, IntValue v) {
    dom->constraints_len++;
    if (dom->constraints == NULL) printf("fixme\n");
    dom->constraints = NULL;
    /*
    IntValue *constraints = (IntValue*)dom->constraints;
    constraints[dom->constraints_len-1] = v;
    dom->constraints = (void*)constraints;*/
}

string int_domain_repr(Domain *dom) {
    switch (dom->t) {
        case DOMAIN: {
            char ls[520];
            strcpy(ls, "(-");
            strcat(ls, INFINITY_STR);

            char rs[520];
            strcpy(rs, INFINITY_STR);
            strcat(rs, ")");

            IntValue l = dom->lborder;
            IntValue r = dom->rborder;

            switch (l.t) {
                case INCLUDED: {

                    char buff[512];
                    bigint_write(buff, 512, l.data);

                    char buff2[520];
                    snprintf(buff2, 520, "[%s", buff);
                    strcpy(ls, buff2);
                    break;
                }
                case SECLUDED: {
                    char buff[512];
                    bigint_write(buff, 512, l.data);

                    char buff2[520];
                    snprintf(buff2, 520, "(%s", buff);
                    strcpy(ls, buff2);
                    break;
                }
                default:
                    break;
            }
            
            switch (r.t) {
                case INCLUDED: {
                    char buff[512];
                    bigint_write(buff, 512, r.data);

                    char buff2[520];
                    snprintf(buff2, 520, "%s]", buff);
                    strcpy(rs, buff2);
                    break;
                }
                case SECLUDED: {
                    char buff[512];
                    bigint_write(buff, 512, r.data);

                    char buff2[520];
                    snprintf(buff2, 520, "%s)", buff);
                    strcpy(rs, buff2);
                    break;
                }
                default:
                    break;
            }

            char buff[1040];
            snprintf(buff, 1040, "%s;%s", ls, rs);

            return strdup(buff);
        }
        case EMPTY:
            return strdup("∅");
    }
}

bigint* int_inc(bigint* n) {
    bigint *bi = malloc(sizeof(bigint));
    bigint_init(bi);
    bigint_cpy(bi, n);
    bigint_add(bi, n, bigint_create(1));
    return bi;
}

bigint* int_dec(bigint* n) {
    bigint *bi = malloc(sizeof(bigint));
    bigint_init(bi);
    bigint_cpy(bi, n);
    bigint_sub(bi, n, bigint_create(1));
    return bi;
}

bigint* int_jump(bigint* n) {
    bigint *bi = malloc(sizeof(bigint));
    bigint_init(bi);
    bigint_cpy(bi, n);
    if (n->neg || bigint_cmp(bi, bigint_create(0)) == 0) {
        bigint_negate(bi);
        bigint_add(bi, bi, bigint_create(1));
    } else {
        bigint_negate(bi);
    }
    return bi;
}


gen_resp int_domain_generate(Domain *dom, gen_resp (*receiver)(bigint*, void*), void* context) {
    switch (dom->t) {
        case DOMAIN: {
            IntValue l = dom->lborder;
            IntValue r = dom->rborder;

            bool from_l = true;
            bool from_r = false;
            switch (l.t) {
                case INFINITE: {
                    from_l = false;
                    break;
                }
                default:
                    break;
            }

            switch (r.t) {
                case INFINITE: {
                    from_r = false;
                    break;
                }
                default:
                    break;
            }

            bigint* value = bigint_create(0);
            bigint* bound = malloc(sizeof(bigint));
            bigint_init(bound);
            bool bounded = true;
            bigint* (*f)(bigint*);

            if (from_l) {
                switch (l.t) {
                    case INCLUDED: {
                        value = l.data;
                        break;
                    }
                    case SECLUDED: {
                        bigint_add(value, l.data, bigint_create(1));
                        break;
                    }
                    default: {
                        break;
                    }
                }
                switch (r.t) {
                    case INCLUDED: {
                        bound = r.data;
                        break;
                    }
                    case SECLUDED: {
                        bigint_sub(bound, r.data, bigint_create(1));
                    }
                        break;
                    default: {
                        bound = 0;
                        bounded = false;
                        break;
                    }
                }
                f = int_inc;
            } else if (from_r) {
                switch (r.t) {
                    case INCLUDED: {
                        value = r.data;
                        break;
                    }
                    case SECLUDED:
                        bigint_sub(value, l.data, bigint_create(1));
                    default: {
                        break;
                    }
                }
                switch (l.t) {
                    case INCLUDED: {
                        bound = l.data;
                        break;
                    }
                    case SECLUDED: {
                        bigint_sub(bound, l.data, bigint_create(1));
                        break;
                    }
                    default: {
                        bound = 0;
                        bounded = false;
                        break;
                    }
                }
                f = int_dec;
            } else {
                bounded = false;
                f = int_jump;
            }

            for (;;) {
                bool not_constrainted = true;

                // check contraints
                for (int i = 0; i < dom->constraints_len; i++) {
                    IntValue *c = ((IntValue**)(dom->constraints))[i];
                    if (bigint_cmp(value, c->data) == 0) {
                        not_constrainted = false;
                        break;
                    }
                }

                if (not_constrainted) {
                    gen_resp gr = (receiver)(value, context);
                    if (gr.r != 1) {
                        return gr;
                    };
                }

                if (bounded) {
                    if (bigint_cmp(value, bound) == 0) {
                        break;
                    }
                }

                value = f(value);
            }
            break;
        }
        case EMPTY:
            break;
    }
    gen_resp r = {1, respond(q_yield, NULL)};
    return r;
}

Domain *int_domain_copy(Domain *dom) {
    Domain *dom_new = malloc(sizeof(Domain));

    //! shallow copy constraints
    dom_new->constraints = dom->constraints;
    dom_new->constraints_len = dom->constraints_len;

    dom_new->lborder = dom->lborder;
    dom_new->rborder = dom->rborder;

    dom_new->t = dom->t;
    return dom_new;
}

void int_domain_free(Domain *dom) {
    if (dom == NULL) return;
    //free(dom);
}

Domain *int_domain_single(bigint *p) {
    Domain *dom = int_domain_new();
    IntValue v1;
    v1.t = INCLUDED;
    v1.data = p;
    IntValue v2;
    v2.t = SECLUDED;
    v2.data = p;
    int_domain_gt(dom, v1);
    int_domain_lt(dom, v2);
    return dom;
}