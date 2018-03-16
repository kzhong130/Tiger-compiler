
/*Lab5: This header file is not complete. Please finish it with more definition.*/

#ifndef FRAME_H
#define FRAME_H

#include "tree.h"

Temp_map F_tempMap;

typedef struct F_accessList_ *F_accessList;

//varibales
struct F_access_ {
	enum {inFrame, inReg} kind;
	union {
		int offset; //inFrame
		Temp_temp reg; //inReg
	} u;
};

struct F_frame_ {
    Temp_label name;
    F_accessList formals;
    int local_cnt;
};



extern const int F_wordSize;

typedef struct F_frame_ *F_frame;

typedef struct F_access_ *F_access;

struct F_accessList_ {F_access head; F_accessList tail;};

F_frame F_newFrame(Temp_label name, U_boolList formals);

Temp_label F_name(F_frame f);
F_accessList F_formals(F_frame f);
F_access F_allocLocal(F_frame f, bool escape);

F_accessList F_AccessList(F_access head, F_accessList tail);


/* declaration for fragments */
typedef struct F_frag_ *F_frag;
struct F_frag_ {enum {F_stringFrag, F_procFrag} kind;
			union {
				struct {Temp_label label; string str;} stringg;
				struct {T_stm body; F_frame frame;} proc;
			} u;
};

F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);

typedef struct F_fragList_ *F_fragList;
struct F_fragList_ 
{
	F_frag head; 
	F_fragList tail;
};

F_fragList F_FragList(F_frag head, F_fragList tail);

T_exp F_externalCall(string s, T_expList args);

Temp_temp F_FP();

T_exp F_Exp(F_access access, T_exp fp);

Temp_temp F_RV();

Temp_temp F_eax();

Temp_temp F_ebx();

Temp_temp F_ecx();

Temp_temp F_edx();

Temp_temp F_esi();

Temp_temp F_edi();

int F_allocSpill(F_frame f);

int F_frameSize(F_frame f);

#endif
