#include "standard.h"
#include <readline/readline.h>

DECLARE(laure_predicate_message) {
    Instance *ins = pd_get_arg(pd, 0);

    cast_image(img, struct ArrayImage) ins->image;

    if (img->t != ARRAY)
        RESPOND_ERROR(type_err, NULL, "message's argument must be array, not %s", IMG_NAMES[img->t]);
    
    if (img->state) {
        array_linked_t *linked = img->i_data.linked;
        for (uint idx = 0; idx < img->i_data.length && linked; idx++) {
            struct CharImage *ch = (struct CharImage*) linked->data->image;
            int c = ch->c;
            if (c == '\\') {
                linked = linked->next;
                idx++;
                c = laure_convert_ch_esc(((struct CharImage*) linked->data->image)->c);
            }
            char buff[8];
            laure_string_put_char(buff, c);
            out("%s", buff);
            linked = linked->next;
        }
        out("\n");
        return True;
    } else {
        string s = readline("> ");
        char buff[128];
        snprintf(buff, 128, "\"%s\"", s);
        laure_free(s);
        laure_expression_t exp[1];
        exp->t = let_custom;
        exp->s = buff;
        bool result = img->translator->invoke(exp, img, cctx->scope, 0);
        return from_boolean(result);
    }
}

#define REPR_MAX_LENGTH 256

DECLARE(laure_predicate_repr) {
    Instance *instance = pd_get_arg(pd, 0);
    Instance *representation = pd->resp;

    if (instance->image == NULL) {
        RESPOND_ERROR(internal_err, NULL, "instance is unknown");
    }

    if (read_head(representation->image).t != ARRAY)
        RESPOND_ERROR(internal_err, NULL, "representation must be array");

    cast_image(repr_ary, struct ArrayImage) representation->image;
    bool instance_inst = instantiated(instance);
    bool repr_known = repr_ary->state == I;

    if (read_head(repr_ary->arr_el->image).t != CHAR)
        RESPOND_ERROR(instance_err, NULL, "must be array of chars");

    if (instance_inst && repr_known) {
        compare_representations: {};
        string instance_representation = instance->repr(instance);
        uint i = 0, len = strlen(instance_representation);
        string temp = instance_representation;

        // remove braces
        while (len > 2 && temp[0] == '(' && lastc(temp) == ')') {
            temp++;
            len -= 2;
        }

        array_linked_t *linked = repr_ary->i_data.linked;
        uint linked_n = repr_ary->i_data.length;
        uint braces_opened = 0;
        bool data_started = false;

        if (linked_n > REPR_MAX_LENGTH)
            RESPOND_ERROR(internal_err, NULL, "too long representation (%d max)", REPR_MAX_LENGTH);

        while (true) {
            if ((! linked) != (! len)) {
                laure_free(instance_representation);
                return False;
            }
            cast_image(cim, struct CharImage) linked->data->image;
            char c = temp[i];
            if (cim->c == '(' && ! data_started) {
                braces_opened++;
                linked = linked->next;
                continue;
            } else data_started = true;
            if (c != cim->c) {
                laure_free(instance_representation);
                return False;
            }
            len--;
            i++;
            linked = linked->next;
            if (! len) {
                laure_free(instance_representation);
                while (braces_opened > 0) {
                    if (! linked || ((struct CharImage*)linked->data->image)->c != ')') {
                        return False;
                    }
                    braces_opened--;
                    linked = linked->next;
                }
                if (linked) {
                    return False;
                } else {
                    return True;
                }
            }
        }
        laure_free(instance_representation);
        return False;
    } else if (instance_inst && ! repr_known) {
        set_representation: {};
        string instance_representation = instance->repr(instance);
        string temp = instance_representation;
        assert(strlen(instance_representation));
        bool braced = false;
        uint len = strlen(instance_representation), wbraces = len - 2;
        if (instance_representation[0] == '(' && lastc(instance_representation) == ')') {
            braced = true;
            temp++;
        }
        // length checks
        bool check = int_domain_check_int(repr_ary->u_data.length, len);
        if (! check && ! braced)
            return False;
        else if (braced) {
            if (! int_domain_check_int(repr_ary->u_data.length, wbraces))
                return False;
        }
        struct ArrayIData i_data;
        if (braced) {
            lastc(temp) = 0;
            i_data = convert_string(temp, cctx->scope);
            temp[strlen(temp)] = ')';
        } else
            i_data = convert_string(instance_representation, cctx->scope);
        
        int_domain_free(repr_ary->u_data.length);
        repr_ary->state = I;
        repr_ary->i_data = i_data;
        return True;
    } else if (repr_known) {
        if (instance->locked)
            goto compare_representations;
        else {
            if (repr_ary->i_data.length > REPR_MAX_LENGTH)
                RESPOND_ERROR(internal_err, NULL, "too long representation (%d max)", REPR_MAX_LENGTH);
            char buff[REPR_MAX_LENGTH];
            memset(buff, '\0', REPR_MAX_LENGTH);
            array_linked_t *linked = repr_ary->i_data.linked;
            uint i = 0;
            while (linked) {
                cast_image(cim, struct CharImage) linked->data->image;
                i += laure_string_put_char_bare(buff + i, cim->c);
                buff[i] = '\0';
                linked = linked->next;
            }
            laure_parse_result lpr = laure_parse(buff);
            if (! lpr.is_ok)
                return False;
            bool result = read_head(instance->image).translator->invoke(lpr.exp, instance->image, cctx->scope, 0);
            //! fixme: parse result is memory leak
            //! todo: proper garbage expression collector
            // maybe there is a need to implement
            // some new unsafe_data datatype to feed it to translators
            return from_boolean(result);
        }
    } else {
        goto set_representation;
    }
    return False;
}

// count arguments in linked formatting
size_t groups_count(struct FormattingPart *first) {
    size_t sz = 0;
    while (first) {
        if (first->name) sz++;
        first = first->next;
    }
    return sz;
}

DECLARE(laure_predicate_format) {
    Instance *formatting_instance = pd_get_arg(pd, 0);
    Instance *string_instance = pd->resp;

    cast_image(fim, struct FormattingImage) formatting_instance->image;
    cast_image(sim, struct ArrayImage) string_instance->image;

    bool string_inst = sim->state == I;

    if (string_inst) {
        // resolve variables if matches
        pattern_element *pattern[128];
        size_t count = 0;
        if (formatting_to_pattern(fim->first, pattern, 128, &count) != 0)
            RESPOND_ERROR(internal_err, NULL, "unable to convert formatting to pattern");
        pattern[count] = 0;
        string buff = laure_alloc(sim->i_data.length + 1);
        if (convert_to_string(sim->i_data, buff) != 0)
            RESPOND_ERROR(internal_err, NULL, "unable to convert string");
        size_t group_count = groups_count(fim->first);
        string *groups = laure_alloc(sizeof(void*) * group_count);
        int result = laure_string_pattern_parse(buff, pattern, groups);
        laure_free(buff);
        for (uint i = 0; i < count; i++)
            laure_free(pattern[i]);
        if (! result) {
            laure_free(groups);
            return False;
        } else {
            struct FormattingPart *part = fim->first;
            uint i = 0;
            laure_scope_t *parent_scope = cctx->scope->next;
            laure_scope_t *parent_scope_owner = parent_scope;
            while (part && part->name) {
                string name = part->name;
                string group = groups[i];

                Instance *instance = laure_scope_find_by_key(
                    parent_scope, 
                    name, 
                    false
                );
                if (! instance) {
                    struct ArrayImage *cpy = image_deepcopy(sim);
                    cpy->i_data = convert_string(group, parent_scope_owner);
                    Instance *new_instance = instance_new(name, NULL, cpy);
                    new_instance->repr = string_instance->repr;
                    laure_scope_insert(parent_scope_owner, new_instance);
                } else if (instance->locked) {
                    goto format_jmp_instantiated_instance;
                } else {
                    if (instantiated(instance)) {
                        format_jmp_instantiated_instance: {};
                        string repr = instance->repr(instance);
                        if (str_eq(repr, group)) {
                            laure_free(groups);
                            return False;
                        }
                    } else {
                        laure_expression_t exp[1];
                        exp->t = let_custom;
                        bool should_free = false;
                        if (read_head(instance->image).translator->invoke == string_translator) {
                            char buff[128];
                            should_free = true;
                            snprintf(buff, 128, "\"%s\"", group);
                            exp->s = strdup(buff);
                        } else {
                            //! add support for arrays
                            exp->s = group;
                        }
                        bool result = read_head(instance->image).translator->invoke(exp, instance->image, parent_scope, 0);
                        if (should_free)
                            laure_free(exp->s);
                        if (! result) {
                            laure_free(groups);
                            return False;
                        }
                    }
                }
                part = part->next;
                i++;
            }
            laure_free(groups);
            return True;
        }
    } else {
        // form string; variables must be known
        struct FormattingPart *part = fim->first;
        char formatted[256];
        strcpy(formatted, "\"");
        while (part) {
            if (part->before) {
                strcat(formatted, part->before);
            }
            if (part->name) {
                Instance *instance = laure_scope_find_by_key(cctx->scope->next, part->name, true);
                if (! instance)
                    RESPOND_ERROR(undefined_err, NULL, "formatting variable %s is undefined", part->name);
                string repr;
                if (instance->repr != string_repr || ! instantiated(instance))
                    repr = instance->repr(instance);
                else {
                    repr = laure_alloc(((struct ArrayImage*)instance->image)->i_data.length + 1);
                    convert_to_string(((struct ArrayImage*)instance->image)->i_data, repr);
                }
                strcat(formatted, repr);
                laure_free(repr);
            }
            part = part->next;
        }
        strcat(formatted, "\"");
        laure_expression_t exp[1];
        exp->t = let_custom;
        exp->s = formatted;
        bool result = sim->translator->invoke(exp, sim, cctx->scope, 0);
        return from_boolean(result);
    }
    return False;
}