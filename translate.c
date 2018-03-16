#include <stdio.h>
#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"

//LAB5: you can modify anything you want.
/*
struct Tr_access_ {
	Tr_level level;
	F_access access;
};



struct Tr_accessList_ {
	Tr_access head;
    Tr_accessList tail;	
};

struct Tr_level_ {
	F_frame frame;
    Tr_level parent;
    Tr_accessList formals;
};

struct Tr_expList_ {
	Tr_exp head;
	Tr_expList tail;	
};

typedef struct patchList_ *patchList;
struct patchList_ 
{
	Temp_label *head; 
	patchList tail;
};

struct Cx 
{
	patchList trues; 
	patchList falses; 
	T_stm stm;
};

struct Tr_exp_ {
	enum {Tr_ex, Tr_nx, Tr_cx} kind;
	union {T_exp ex; T_stm nx; struct Cx cx; } u;
};
*/

static Tr_exp Tr_Ex(T_exp ex)
{
    Tr_exp res = checked_malloc(sizeof(*res));
    res->kind = Tr_ex;
    res->u.ex = ex;
    return res;
}

static Tr_exp Tr_Nx(T_stm nx)
{
    Tr_exp res = checked_malloc(sizeof(*res));
    res->kind = Tr_nx;
    res->u.nx = nx;
    return res;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm)
{
    Tr_exp res = checked_malloc(sizeof(*res));
    res->kind = Tr_cx;
    res->u.cx.trues = trues;
    res->u.cx.falses = falses;
    res->u.cx.stm = stm;
    return res;
}


Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail)
{
    Tr_expList res = checked_malloc(sizeof(*res));
    res->head = head;
    res->tail = tail;
    return res;
}



Tr_access Tr_Access(Tr_level level, F_access access)
{
    Tr_access res = checked_malloc(sizeof(*res));
    res->level = level;
    res->access = access;
    return res;
}

Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail)
{
    Tr_accessList res = checked_malloc(sizeof(*res));
    res->head = head;
    res->tail = tail;
    return res;
}

Tr_accessList makeLevelFormals(Tr_level level, F_accessList l)
{
    if (l) {
        fprintf(stdout,"access kind %d offs %d\n",l->head->kind,l->head->u.offset);
        return Tr_AccessList(Tr_Access(level, l->head), makeLevelFormals(level, l->tail));
    } else {
        return NULL;
    }
}


Tr_accessList Tr_formals(Tr_level level)
{
    return level->formals;
}



static Tr_level outermost = NULL;
Tr_level Tr_outermost(void)
{
    if (outermost) {
        return outermost;
    } else {
        outermost = checked_malloc(sizeof(*outermost));
        outermost->parent = NULL;
        outermost->frame = F_newFrame(Temp_namedlabel("tigermain"), NULL);
        outermost->formals = NULL;
        return outermost;
    }
}

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals)
{
    Tr_level res = checked_malloc(sizeof(*res));
    res->parent = parent;

    //treat every var as escape 
    fprintf(stdout,"new level %s\n",S_name(name));
    res->frame = F_newFrame(name, U_BoolList(1, formals));
    res->formals = makeLevelFormals(res, F_formals(res->frame)->tail); 
    return res;
}

//alloc local var
Tr_access Tr_allocLocal(Tr_level level, bool escape)
{
    //frame->local_var is used to record the offset of var
    fprintf(stdout,"he\n");
    return Tr_Access(level, F_allocLocal(level->frame, escape));
}

static F_fragList fragList;


F_fragList Tr_getResult(void) {
    return fragList;
}


static patchList PatchList(Temp_label *head, patchList tail)
{
	patchList list;

	list = (patchList)checked_malloc(sizeof(struct patchList_));
	list->head = head;
	list->tail = tail;
	return list;
}

void doPatch(patchList tList, Temp_label label)
{
	for(; tList; tList = tList->tail)
		*(tList->head) = label;
}

patchList joinPatch(patchList first, patchList second)
{
	if(!first) return second;
	for(; first->tail; first = first->tail);
	first->tail = second;
	return first;
}





//translate part

static T_exp unEx(Tr_exp e) {
    switch(e->kind) {
        case Tr_ex:
            return e->u.ex;
        case Tr_cx:
            {
                Temp_temp r = Temp_newtemp();
                Temp_label t = Temp_newlabel();
                Temp_label f = Temp_newlabel();
                doPatch(e->u.cx.trues, t);
                doPatch(e->u.cx.falses, f);
                return T_Eseq(T_Move(T_Temp(r), T_Const(1)),
                           T_Eseq(e->u.cx.stm,
                               T_Eseq(T_Label(f),
                                   T_Eseq(T_Move(T_Temp(r), T_Const(0)),
                                       T_Eseq(T_Label(t), T_Temp(r))))));
            }
        case Tr_nx:
            return T_Eseq(e->u.nx, T_Const(0));
    }
    assert(0); /* can't get here */
}

static T_stm unNx(Tr_exp e) {
    switch(e->kind) {
        case Tr_ex:
            return T_Exp(e->u.ex);
        case Tr_cx:
            {
                Temp_label l = Temp_newlabel();
                doPatch(e->u.cx.trues, l);
                doPatch(e->u.cx.falses, l);
                return T_Seq(e->u.cx.stm, T_Label(l));
            }
        case Tr_nx:
            return e->u.nx;
    }
    assert(0); /* can't get here */
}

static struct Cx unCx(Tr_exp e) {
    switch(e->kind) {
        case Tr_ex:
            {
                struct Cx res;
                res.stm = T_Cjump(T_ne, unEx(e), T_Const(0), NULL, NULL);
                res.trues = PatchList(&(res.stm->u.CJUMP.true), NULL);
                res.falses = PatchList(&(res.stm->u.CJUMP.false), NULL);
                return res;
            }
        case Tr_cx:
            return e->u.cx;
        case Tr_nx:
            assert(0); 
    }
    assert(0); 
}

static T_exp staticLink(Tr_level now, Tr_level target)
{
    T_exp res = T_Temp(F_FP());
    Tr_level cur = now;
    while (cur && cur != target) {
        res = F_Exp(F_formals(cur->frame)->head, res);
        cur = cur->parent;
    }
    return res;
}


Tr_exp Tr_Nil()
{
    return Tr_Ex(T_Const(0)); 
}


Tr_exp Tr_Nop()
{
    return Tr_Ex(T_Const(0));
}



Tr_exp Tr_Int(int v)
{
    return Tr_Ex(T_Const(v));
}

Tr_exp Tr_String(string s)
{
    Temp_label l = Temp_newlabel();
    F_frag frag = F_StringFrag(l, s);
    fragList = F_FragList(frag, fragList);
    return Tr_Ex(T_Name(l));
}
    

T_stm Tr_procEntryExit(Tr_level level, Tr_exp body, Tr_accessList formals)
{
    //store result in eax
    return T_Move(T_Temp(F_RV()), unEx(body));
}



void Tr_Func(Tr_exp body, Tr_level level) 
{
    T_stm b = Tr_procEntryExit(level, body, level->formals);
    F_frag frag = F_ProcFrag(b, level->frame);
    fragList = F_FragList(frag, fragList);
}


Tr_exp Tr_simpleVar(Tr_access access, Tr_level level)
{
    //return Tr_Nil(); 
    T_exp fp = staticLink(level, access->level);
    //fprintf("");
    return Tr_Ex(F_Exp(access->access, fp));
}

Tr_exp Tr_fieldVar(Tr_exp addr, int offset)
{
    //offset compared to the sp
    return Tr_Ex(T_Mem(T_Binop(T_plus,
                unEx(addr),
                    T_Const(offset * F_wordSize))));
}

Tr_exp Tr_subscriptVar(Tr_exp addr, Tr_exp offset)
{
    return Tr_Ex(T_Mem(T_Binop(T_plus, 
                unEx(addr),
                    T_Binop(T_mul, unEx(offset), T_Const(F_wordSize)))));
    
}


Tr_exp Tr_Op(A_oper oper, Tr_exp left, Tr_exp right)
{
    T_exp lexp = unEx(left);
    T_exp rexp = unEx(right);
    T_exp result;
    switch (oper) {
        case A_plusOp:
            result = T_Binop(T_plus, lexp, rexp);
            break;
        case A_minusOp:
            result = T_Binop(T_minus, lexp, rexp);
            break;
        case A_timesOp:
            result = T_Binop(T_mul, lexp, rexp);
            break;
        case A_divideOp:
            result = T_Binop(T_div, lexp, rexp);
            break;
    }
    return Tr_Ex(result);
}



Tr_exp Tr_OpCmp(A_oper oper, Tr_exp left, Tr_exp right, int flag)
{
    T_exp l;
    T_exp r;
    struct Cx res;
    if (flag) {
        //use system call stringEqual 
        l = F_externalCall("stringEqual", T_ExpList(unEx(left), T_ExpList(unEx(right), NULL)));
        r = T_Const(0);
    } 
    else {
        l = unEx(left);
        r = unEx(right);
    }
    switch (oper) {
        case A_eqOp:
            res.stm = T_Cjump(T_eq, l, r, NULL, NULL);
            break;
        case A_neqOp:
            res.stm = T_Cjump(T_ne, l, r, NULL, NULL);
            break;
        case A_ltOp:
            res.stm = T_Cjump(T_lt, l, r, NULL, NULL);
            break;
        case A_gtOp:
            res.stm = T_Cjump(T_gt, l, r, NULL, NULL);
            break;
        case A_leOp:
            res.stm = T_Cjump(T_le, l, r, NULL, NULL);
            break;
        case A_geOp:
            res.stm = T_Cjump(T_ge, l, r, NULL, NULL);
            break;
    }
    res.trues = PatchList(&(res.stm->u.CJUMP.true), NULL);
    res.falses = PatchList(&(res.stm->u.CJUMP.false), NULL);
    return Tr_Cx(res.trues, res.falses, res.stm);
}

T_stm assignRecord(Temp_temp r, int num, Tr_expList fields)
{
    if (fields != NULL) {
        return T_Seq(T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(F_wordSize * num))),
                    unEx(fields->head)),
                        assignRecord(r, num - 1, fields->tail));
    } else {
        return T_Exp(T_Const(0));
    }
}


Tr_exp Tr_Record(int num, Tr_expList fields)
{
    Temp_temp r = Temp_newtemp();
    
    return Tr_Ex(T_Eseq(T_Move(T_Temp(r), F_externalCall("allocRecord", T_ExpList(T_Const(F_wordSize * num), NULL))),
                T_Eseq(assignRecord(r, num - 1, fields),T_Temp(r)) //sequentially move the results to r[0] r[1] etc
                ));
}

Tr_exp Tr_Seq(Tr_exp seq, Tr_exp e)
{
    return Tr_Ex(T_Eseq(unNx(seq), unEx(e)));
}

Tr_exp Tr_Assign(Tr_exp var, Tr_exp value)
{
    return Tr_Nx(T_Move(unEx(var), unEx(value)));
}


Tr_exp Tr_IfThen(Tr_exp cond, Tr_exp then)
{
    Temp_label t = Temp_newlabel();
    Temp_label f = Temp_newlabel();
    struct Cx cx = unCx(cond);
    doPatch(cx.trues, t);
    doPatch(cx.falses, f);
    
    //T_stm un_then=unNx(then);
    return Tr_Nx(T_Seq(cx.stm,T_Seq(T_Label(t),
                    T_Seq(unNx(then),T_Label(f)))));

}

Tr_exp Tr_IfThenElse(Tr_exp cond, Tr_exp then, Tr_exp elsee)
{
    Temp_temp r = Temp_newtemp();
    Temp_label t = Temp_newlabel();
    Temp_label f = Temp_newlabel();
    Temp_label done = Temp_newlabel();
    struct Cx cx = unCx(cond);
    doPatch(cx.trues, t);
    doPatch(cx.falses, f);
    /*
    T_exp ex_then = unEx(then);
    T_exp ex_elsee = unEx(elsee);
    return Tr_Ex(
            T_Eseq(cx.stm,
                T_Eseq(T_Label(t),
                    T_Eseq(T_Move(T_Temp(r), ex_then),T_Eseq(T_Jump(T_Name(done), Temp_LabelList(done, NULL)),
                        T_Eseq(T_Label(f),
                            T_Eseq(T_Move(T_Temp(r), ex_elsee),T_Eseq(T_Jump(T_Name(done), Temp_LabelList(done, NULL)),
                                T_Eseq(T_Label(done), T_Temp(r))))))))));
                                */
    return Tr_Ex(T_Eseq(cx.stm,
            T_Eseq(T_Label(t),
                T_Eseq(T_Move(T_Temp(r), unEx(then)),
                    T_Eseq(T_Jump(T_Name(done), Temp_LabelList(done, NULL)),
                        T_Eseq(T_Label(f),
                            T_Eseq(T_Move(T_Temp(r), unEx(elsee)),
                                T_Eseq(T_Jump(T_Name(done), Temp_LabelList(done, NULL)),
                                    T_Eseq(T_Label(done), T_Temp(r))))))))));

}


Tr_exp Tr_While(Tr_exp cond, Tr_exp body, Temp_label done)
{
    Temp_label t = Temp_newlabel();
    Temp_label b = Temp_newlabel();
    struct Cx cx = unCx(cond);
    doPatch(cx.trues, b);
    doPatch(cx.falses, done);
   
    T_stm nx_body = unNx(body);
    return Tr_Nx(T_Seq(T_Label(t),
                T_Seq(cx.stm,
                    T_Seq(T_Label(b),
                        T_Seq(nx_body,T_Seq(T_Jump(T_Name(t), Temp_LabelList(t, NULL)),
                            T_Label(done)))))));
}


Tr_exp Tr_For(Tr_access access, Tr_level level, Tr_exp lo, Tr_exp hi, Tr_exp body, Temp_label done)
{
    Tr_exp i = Tr_simpleVar(access, level);
    Temp_temp limit = Temp_newtemp();
    Temp_label b = Temp_newlabel();
    Temp_label inc = Temp_newlabel();
   
    T_exp loexp = unEx(lo);
    T_exp hiexp = unEx(hi);
    T_stm bodystm = unNx(body);
    T_exp indexExp = unEx(i);  //same label should call unEx only once?

    return Tr_Nx(T_Seq(T_Move(indexExp, loexp),
                T_Seq(T_Move(T_Temp(limit), hiexp),
                    T_Seq(T_Cjump(T_gt, indexExp, T_Temp(limit), done, b),
                        T_Seq(T_Label(b),
                            T_Seq(bodystm,
                                T_Seq(T_Cjump(T_eq, indexExp, T_Temp(limit), done, inc),
                                    T_Seq(T_Label(inc),
                                        T_Seq(T_Move(indexExp, T_Binop(T_plus, indexExp, T_Const(1))),//i++
                                            T_Seq(T_Jump(T_Name(b), Temp_LabelList(b, NULL)),
                                                T_Label(done)))))))))));
}


Tr_exp Tr_Jump(Temp_label l)
{
    return Tr_Nx(T_Jump(T_Name(l), Temp_LabelList(l, NULL)));
}

Tr_exp Tr_Array(Tr_exp size, Tr_exp init)
{
    //use internal function
    
    return Tr_Ex(F_externalCall(String("initArray"),
                T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
}


Tr_exp Tr_Call(Tr_level level, Temp_label label, Tr_expList params, Tr_level cur)
{
    T_expList res = NULL;
    Tr_expList p = params;
    while (p) {
        res = T_ExpList(unEx(p->head), res);
        p = p->tail;
    }
    string name = Temp_labelstring(label);
    if (level == NULL) {
        return Tr_Ex(F_externalCall(name, res));
    } 
    else {
        return Tr_Ex(T_Call(T_Name(label), T_ExpList(staticLink(cur, level->parent), res)));
    }    
    //return Tr_Ex(T_Call(T_Name(label), T_ExpList(staticLink(cur, level->parent), res)));
}

