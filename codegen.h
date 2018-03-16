#ifndef CODEGEN_H
#define CODEGEN_H

AS_instrList F_codegen(F_frame f, T_stmList stmList);

static void emit(AS_instr inst);

static void munchStm(T_stm s);

static Temp_temp munchExp(T_exp e);

#endif
