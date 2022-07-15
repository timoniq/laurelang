#include "standard.h"
#include <readline/readline.h>

qresp laure_predicate_message(preddata *pd, control_ctx *cctx) {
    Instance *ins = pd_get_arg(pd, 0);
    struct ArrayImage *img = (struct ArrayImage*) ins->image;
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
            printf("%s", buff);
            linked = linked->next;
        }
        printf("\n");
        return respond(q_true, 0);
    } else {
        string s = readline("> ");
        char buff[128];
        snprintf(buff, 128, "\"%s\"", s);
        free(s);
        laure_expression_t exp[1];
        exp->t = let_custom;
        exp->s = buff;
        bool result = img->translator->invoke(exp, img, cctx->scope);
        return respond(result ? q_true : q_false, 0);
    }
}