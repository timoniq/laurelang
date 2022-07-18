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
        free(s);
        laure_expression_t exp[1];
        exp->t = let_custom;
        exp->s = buff;
        bool result = img->translator->invoke(exp, img, cctx->scope);
        return from_boolean(result);
    }
}

#define REPR_MAX_LENGTH 256

DECLARE(laure_predicate_repr) {
    Instance *instance = pd_get_arg(pd, 0);
    Instance *representation = pd->resp;

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
                free(instance_representation);
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
                free(instance_representation);
                return False;
            }
            len--;
            i++;
            linked = linked->next;
            if (! len) {
                free(instance_representation);
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
        free(instance_representation);
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
            laure_expression_t exp[1];
            exp->s = strdup(buff);
            exp->t = let_custom;
            bool result = read_head(instance->image).translator->invoke(exp, instance->image, cctx->scope);
            if (! result) free(exp->s);
            return from_boolean(result);
        }
    } else {
        goto set_representation;
    }
    printf("err\n");
    return False;
}