#ifndef ESCAPE_H
#define ESCAPE_H

typedef struct escapeEntry_ *escapeEntry;

struct escapeEntry_ {
    int depth;
    bool* escape;
};


void Esc_findEscape(A_exp exp);

static void traverseExp(S_table env, int depth, A_exp e);
static void traverseDec(S_table env, int depth, A_dec d);
static void traverseVar(S_table env, int depth, A_var v);

#endif
