
#define string char*
#define lastc(s) s[strlen(s)-1]
#define str_eq(s1, s2) (s1 != NULL && s2 != NULL && strcmp(s1, s2) == 0)
#define str_starts(s, start) (strncmp(start, s, strlen(start)) == 0)