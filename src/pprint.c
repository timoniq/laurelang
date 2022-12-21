#include "laurelang.h"

string point_substr(string str, string substr) {
    int i = 0;

    size_t str_len = laure_string_strlen(str);
    size_t substr_len = laure_string_strlen(substr);

    for (; i < str_len - substr_len; i++) {
       bool is_valid = true;
        for (int j = 0; j < laure_string_strlen(substr); j++) {
            int cc = laure_string_char_at_pos(substr, substr_len, j);
            if (laure_string_char_at_pos(str, str_len, i + j) != cc) {
                is_valid = false;
                break;
            }
        }
        if (is_valid) {
            return str + i;
        }
    }
    return str + str_len;
}

void laure_pprint_doc(string content, size_t docindent) {
    char buff[8];
    bool is_code = false;
    bool is_escaped = false;
    size_t l = laure_string_strlen(content);
    for (int j = 0; j < docindent; j++) {
        printf(" ");
    }
    for (int i = 0; i < laure_string_strlen(content); i++) {

        int c = laure_string_char_at_pos(content, l, i);
        laure_string_put_char(buff, c);

        if (is_escaped) {
            is_escaped = false;
            printf("%s", buff);
            continue;
        }
        switch (c) {
            case '\\': {
                is_escaped = true;
                break;
            }
            case '`': {
                bool cont;
                string after;
                if (i >= l - 1) {
                    continue;
                } else if (
                    laure_string_strlen(content) - i >= 2
                    && laure_string_char_at_pos(content, l, i + 1) == '`'
                    && laure_string_char_at_pos(content, l, i + 2) == '`'
                ) {
                    // code block
                    printf("\r");
                    string codeblock = content + i + 3;
                    after = point_substr(codeblock, "```");
                    cont = (*after) != 0;
                    char ch = *after;
                    *after = 0;
                    // now code block and codeline do not have any difference
                    // but perhaps \n could be banned in codeline (?)
                    laure_pprint_code(codeblock, docindent);
                    *after = ch;
                    after = after + 3;
                } else {
                    // code line
                    string codeline = content + i + 1;
                    after = point_substr(content + i + 1, "`");
                    cont = (*after) != 0;
                    char ch = *after;
                    *after = 0;
                    laure_pprint_code(codeline, 0);
                    *after = ch;
                    after = after + 1;
                }
                if (! cont) {
                    printf("\n");
                    return;
                }
                i = (int)(after - content) - 1;
                break;
            }
            case '*': {

                if ((i >= l - 1) || (laure_string_char_at_pos(content, l, i + 1) != '*')) {
                    printf("*");
                    break;
                }

                string bold = content + i + 2;
                string after = point_substr(bold, "**");
                bool cont = (*after) != 0;
                char ch = *after;
                *after = 0;
                printf("%s%s%s", BOLD_WHITE, bold, NO_COLOR);
                *after = ch;
                after = after + 2;
                if (! cont) {
                    printf("\n");
                    return;
                }
                i = (int)(after - content) - 1;
                break;
            }
            case '\n': {
                printf("\n");
                for (int j = 0; j < docindent; j++) {
                    printf(" ");
                }
                break;
            }
            default: {
                printf("%s", buff);
                break;
            }
        }
    }
    printf("\n");
}

void laure_pprint_code(string content, size_t indent) {
    // TODO
    for (int j = 0; j < indent; j++)
        printf(" ");
    
    printf("%s%s%s", LAURUS_NOBILIS, content, NO_COLOR);
}