#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "helper.h"
#include "env.h"
#include "semant.h"

/*Lab4: Your implementation of lab4*/

string for_index = NULL;

//typedef void* Tr_exp;
struct expty 
{
	Tr_exp exp; 
	Ty_ty ty;
};

//In Lab4, the first argument exp should always be **NULL**.
struct expty expTy(Tr_exp exp, Ty_ty ty)
{
	struct expty e;

	e.exp = exp;
	e.ty = ty;

	return e;
}

//skip the Ty_name
Ty_ty actual_ty(Ty_ty ty) 
{
    if (ty->kind == Ty_name && ty->u.name.ty != NULL) {
        return ty->u.name.ty;
	} 
	else {
        return ty;
    }
}

Ty_tyList makeFormalTyList(A_pos pos, S_table tenv, A_fieldList fl)
{
    if (fl) {
		Ty_ty ty = S_look(tenv, fl->head->typ);
		if(ty == NULL){	
			EM_error(pos, "undefined type %s", S_name(fl->head->typ));
			ty = Ty_Int();
		}
		ty = actual_ty(ty);
        return Ty_TyList(ty, makeFormalTyList(pos,tenv, fl->tail));
	} 
	else {
        return NULL;
    }
}

Ty_fieldList makeTyFieldList(A_pos pos, S_table tenv, A_fieldList l)
{
    if (l) {
		Ty_ty ty = S_look(tenv, l->head->typ);
		if(ty == NULL){	
			EM_error(pos, "undefined type %s", S_name(l->head->typ));
			ty = Ty_Int();
		}
		ty = actual_ty(ty);
		//EM_error(1,"mkf %d",ty->kind);
        return Ty_FieldList(Ty_Field(l->head->name, ty), makeTyFieldList(pos,tenv, l->tail));
	} 
	else {
        return NULL;
    }
}

Ty_fieldList actual_tys(Ty_fieldList l)
{
    if (l) {
		Ty_ty ty = actual_ty(l->head->ty);
		//EM_error(1,"type %d",ty->kind);
        return Ty_FieldList(Ty_Field(l->head->name, ty), actual_tys(l->tail));
	}
	else {
        return NULL;
    }
}


U_boolList makeFormalBoolList(A_fieldList l)
{
    if (l) {
        return U_BoolList(l->head->escape, makeFormalBoolList(l->tail));
    } else {
        return NULL;
    }
}

struct expty transVar(S_table venv, S_table tenv, A_var v,Tr_level level, Temp_label loop)
{
	switch(v->kind){
		case A_simpleVar:
		{
			E_enventry x = S_look(venv, v->u.simple);
			if (x != NULL && x->kind == E_varEntry) {
				fprintf(stdout,"var %s\n",S_name(v->u.simple));
				Tr_exp var=Tr_simpleVar(x->u.var.access, level);
				fprintf(stdout,"offset %d\n",x->u.var.access->access->u.offset);
				return expTy(var, x->u.var.ty);
			}
			else {
				EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
				return expTy(Tr_Nop(), Ty_Int());
			}
			break;
		}
		
		case A_fieldVar:
		{
			struct expty var = transVar(venv, tenv, v->u.field.var,level,loop);
			if (var.ty->kind != Ty_record) {
				EM_error(v->u.field.var->pos, "not a record type");
				return expTy(Tr_Nop(), Ty_Int());
			}
			else {
				int offset=0;
				for (Ty_fieldList field = var.ty->u.record; field != NULL; field = field->tail) {
					if (field->head->name == v->u.field.sym) {
						return expTy(Tr_fieldVar(var.exp, offset),field->head->ty);
					}
					offset++;
				}
				EM_error(v->u.field.var->pos, "field %s doesn't exist", S_name(v->u.field.sym));
				return expTy(Tr_Nop(), Ty_Int());
			}
			break;
		}

		case A_subscriptVar:
		{
			struct expty var = transVar(venv, tenv,v->u.subscript.var,level,loop);
			if (var.ty->kind != Ty_array) {
				EM_error(v->u.subscript.var->pos, "array type required");
				return expTy(Tr_Nop(), Ty_Int());
			}
			else {
				struct expty exp = transExp(venv, tenv, v->u.subscript.exp,level,loop);
				if (exp.ty->kind != Ty_int) {
					EM_error(v->u.subscript.exp->pos, "integer required");
				}
				return expTy(Tr_subscriptVar(var.exp, exp.exp), var.ty->u.array);
			}
			break;
		}
	}
}

struct expty transExp(S_table venv, S_table tenv, A_exp a, Tr_level level, Temp_label loop)
{
	switch(a->kind) {
        case A_varExp:
            return transVar(venv, tenv, a->u.var,level,loop);
        case A_nilExp:
            return expTy(Tr_Nil(), Ty_Nil());
        case A_intExp:
            return expTy(Tr_Int(a->u.intt), Ty_Int());
        case A_stringExp:
            return expTy(Tr_String(a->u.stringg), Ty_String());
		case A_callExp:
		{
			A_expList arg;
			Ty_tyList formal;
			struct expty exp;
			//look up by func name
			//EM_error(a->pos, "call function %s", S_name(a->u.call.func));			
			E_enventry x = S_look(venv, a->u.call.func);
			Tr_expList params = NULL;			
			if (x !=NULL) {
				for (arg = a->u.call.args, formal = x->u.fun.formals; arg && formal; arg = arg->tail, formal = formal->tail) {
					exp = transExp(venv, tenv, arg->head,level,NULL);
					if (formal->head->kind != exp.ty->kind) {
						EM_error(arg->head->pos, "para type mismatch");
					}
					//
					params = Tr_ExpList(exp.exp, params);					
				}
				if (arg) {
					EM_error(a->pos, "too many params in function %s",S_name(a->u.call.func));
				}
				if (formal) {
					EM_error(a->pos, "too few params in function %s",S_name(a->u.call.func));
				}
				/// TODO
				return expTy(Tr_Call(x->u.fun.level, x->u.fun.label, params, level), x->u.fun.result);			
			} 
			
			else {
				/*
				if(x == NULL){
					EM_error(a->pos, "u %s", S_name(a->u.call.func));
				}
				if(x != NULL && x->kind != E_funEntry){
					EM_error(a->pos, "ku %d", x->u.var.ty->kind);
				}
				*/
				EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
				return expTy(Tr_Nop(),Ty_Int());
			}
			break;
		}

		case A_opExp:
		{
			A_oper oper = a->u.op.oper;
			struct expty left = transExp(venv, tenv, a->u.op.left,level,loop);
			struct expty right = transExp(venv, tenv, a->u.op.right,level,loop);
			if (oper == A_plusOp || oper == A_minusOp || oper == A_timesOp || oper == A_divideOp) {
				if (left.ty->kind != Ty_int) {
					EM_error(a->u.op.left->pos, "integer required");
				}
				if (right.ty->kind != Ty_int) {
					EM_error(a->u.op.right->pos, "integer required");
				}
				return expTy(Tr_Op(oper, left.exp, right.exp),Ty_Int());
			}
			else {
				if (oper == A_eqOp || oper == A_neqOp) {

					// right to be nil is OK
					if (right.ty->kind != Ty_nil && (left.ty->kind != right.ty->kind || left.ty->kind == Ty_void)) {
						EM_error(a->pos, "same type required");
					}
					int flag=0;
					if( left.ty->kind == Ty_string) flag=1;
					return expTy(Tr_OpCmp(oper, left.exp, right.exp, flag), Ty_Int());
				} 
				else {
					if (!((left.ty->kind == Ty_int && right.ty->kind == Ty_int) || (left.ty->kind == Ty_string && right.ty->kind == Ty_string))) {
						EM_error(a->pos, "same type required");
					}
					int flag=0;
					if( left.ty->kind == Ty_string) flag=1;
					return expTy(Tr_OpCmp(oper, left.exp, right.exp, flag), Ty_Int());
				}
			}
			break;
		}

		case A_recordExp:
		{
			Ty_ty search_type = S_look(tenv, a->u.record.typ);
			if(search_type == NULL){	
				EM_error(a->pos, "undefined type %s", S_name(a->u.record.typ));
				search_type = Ty_Int();
			}
			
			Ty_ty ty = actual_ty(search_type);
			A_efieldList ef;
			Ty_fieldList f;
			Tr_expList tfields = NULL;			
			struct expty exp;
			if (ty->kind != Ty_record) {
				EM_error(a->pos, "record type required");
				return expTy(Tr_Nil(), Ty_Int());
			}

			int num = 0;

			for (ef = a->u.record.fields, f = ty->u.record; ef && f; ef = ef->tail, f = f->tail) {
				exp = transExp(venv, tenv, ef->head->exp,level,loop);
				if (exp.ty->kind != Ty_nil && !(ef->head->name == f->head->name && f->head->ty == exp.ty)) {
				//if (!(ef->head->name == f->head->name && f->head->ty->kind == exp.ty->kind)) {
					EM_error(ef->head->exp->pos, "type mismatch");
				}
				num++;
				tfields = Tr_ExpList(exp.exp, tfields);				
			}
			if ( f != NULL || ef != NULL) {
				EM_error(a->pos, "type mismatch");
			}
			return expTy(Tr_Record(num, tfields), ty);

		}

		case A_seqExp:
		{
			A_expList list;
			Tr_exp exp = Tr_Nop();
			Ty_ty ty=Ty_Void();			
			struct expty res;
			for (list = a->u.seq; list != NULL; list = list->tail) {
				//EM_error(0,"%d",list->head->kind);				
				res = transExp(venv, tenv, list->head,level,loop);
				ty = res.ty;
				exp = Tr_Seq(exp, res.exp);
			}
			res = expTy(exp, ty);	
			//res = expTy(exp, Ty_Void());
			return res;
		}

		case A_assignExp: 
		{
			struct expty var_ty = transVar(venv, tenv, a->u.assign.var,level,loop);
			struct expty exp_ty = transExp(venv, tenv, a->u.assign.exp,level,loop);
			
			if (a->u.assign.var->kind == A_simpleVar && for_index != NULL && !strcmp(S_name(a->u.assign.var->u.simple), for_index)) {
				EM_error(a->pos, "loop variable can't be assigned");
			}

			if (exp_ty.ty->kind != Ty_nil && var_ty.ty->kind != exp_ty.ty->kind) {
				EM_error(a->pos, "unmatched assign exp");
			}
			
			return expTy(Tr_Assign(var_ty.exp, exp_ty.exp),Ty_Void());
		}

		case A_ifExp:
		{
			struct expty cond = transExp(venv, tenv, a->u.iff.test,level,loop);
			if (cond.ty->kind != Ty_int) {
				EM_error(a->u.iff.test->pos, "if Exp require integer");
			}
			struct expty then = transExp(venv, tenv, a->u.iff.then,level,loop);
			if (a->u.iff.elsee != NULL) {
				struct expty elsee = transExp(venv, tenv, a->u.iff.elsee,level,loop);
				//nil is oK
				if (elsee.ty->kind != Ty_nil && then.ty->kind != elsee.ty->kind) {
					EM_error(a->pos, "then exp and else exp type mismatch");
				}
				return expTy(Tr_IfThenElse(cond.exp, then.exp, elsee.exp), then.ty);
			} 
			else {
				//no else
				if (then.ty->kind != Ty_void) {
					EM_error(a->pos, "if-then exp's body must produce no value");
				}
				return expTy(Tr_IfThen(cond.exp, then.exp), Ty_Void());
			}
		}

		case A_whileExp:
		{
			Temp_label done = Temp_newlabel();
			struct expty cond = transExp(venv, tenv, a->u.whilee.test,level,loop);
			struct expty body = transExp(venv, tenv, a->u.whilee.body,level,done);
			if (cond.ty->kind != Ty_int) {
				EM_error(a->u.whilee.test->pos, "integer required");
			}
			if (body.ty->kind != Ty_void) {
				EM_error(a->u.whilee.body->pos, "while body must produce no value");
			}
			return expTy(Tr_While(cond.exp, body.exp, done), Ty_Void());
		}

		case A_forExp:
		{
			Temp_label done = Temp_newlabel();
			struct expty lo = transExp(venv, tenv, a->u.forr.lo,level,loop);
			struct expty hi = transExp(venv, tenv, a->u.forr.hi,level,loop);
			struct expty body;
			if (lo.ty->kind != Ty_int) {
				EM_error(a->u.forr.lo->pos, "for exp's range type is not integer");
			}
			if (hi.ty->kind != Ty_int) {
				EM_error(a->u.forr.hi->pos, "for exp's range type is not integer");
			}
			S_beginScope(venv);
			Tr_access access = Tr_allocLocal(level, a->u.forr.escape);
			
			//add the loop variable to the env
			for_index = S_name(a->u.forr.var);
			S_enter(venv, a->u.forr.var, E_VarEntry(access,Ty_Int()));
			body = transExp(venv, tenv, a->u.forr.body,level,done);
			if (body.ty->kind != Ty_void) {
				EM_error(a->u.forr.body->pos, "while body must produce no value");
			}
			S_endScope(venv);
			for_index=NULL;
			return expTy(Tr_For(access, level, lo.exp, hi.exp, body.exp, done),Ty_Void());
		}

		case A_breakExp:
		{
			//return expTy(Tr_Nil(),Ty_Void());
			if (loop == NULL) {
				EM_error(a->pos, "break is not inside a loop");
				return expTy(Tr_Nop(), Ty_Void());
			} else {
				return expTy(Tr_Jump(loop), Ty_Void());
			}
		
		}

		case A_letExp:
		{
			struct expty exp;
			Tr_exp e = Tr_Nop();			
			A_decList d;
			S_beginScope(venv);
			S_beginScope(tenv);
		
			for (d = a->u.let.decs; d; d = d->tail) {
				e = Tr_Seq(e, transDec(venv, tenv, d->head, level, loop));
			}
			exp = transExp(venv, tenv, a->u.let.body,level,loop);
			exp.exp = Tr_Seq(e, exp.exp);			
			S_endScope(tenv);
			S_endScope(venv);
			return exp;
		}

		case A_arrayExp:
		{
			Ty_ty search_type = S_look(tenv, a->u.array.typ);
			if(search_type == NULL){	
				EM_error(a->pos, "undefined type %s", S_name(a->u.array.typ));
				search_type = Ty_Int();
			}
			Ty_ty ty = actual_ty(search_type);
			struct expty size = transExp(venv, tenv, a->u.array.size,level,loop);
			struct expty init = transExp(venv, tenv, a->u.array.init,level,loop);
			if (ty->kind != Ty_array) {
				EM_error(a->pos, "array type required");
				return expTy(Tr_Nil(),Ty_Int());
			}
			if (size.ty->kind != Ty_int) {
				EM_error(a->u.array.size->pos, "integer required");
			}
			
			if (ty->u.array != init.ty) {
				EM_error(a->u.array.init->pos, "type mismatch");
			}
			return expTy(Tr_Array(size.exp, init.exp),ty);
		}
	}
}

Tr_exp transDec(S_table venv, S_table tenv, A_dec d, Tr_level level, Temp_label loop)
{
	switch(d->kind){
		case A_varDec:
		{
			//EM_error(d->pos, "var dec name %s", S_name(d->u.var.var));
			
			struct expty init_exp = transExp(venv, tenv,d->u.var.init,level,loop);
			Tr_access access = Tr_allocLocal(level, d->u.var.escape);
			fprintf(stdout,"declare var %s \n",S_name(d->u.var.var));			
			if(access->access->kind==0){
				fprintf(stdout,"declare var %s offset %d\n",S_name(d->u.var.var),access->access->u.offset);
			}
			//check if initialize with type declaration
			if (d->u.var.typ != S_Symbol("")) {
				//EM_error(d->pos, "var name %s", S_name(d->u.var.var));
				Ty_ty spec_ty = S_look(tenv,d->u.var.typ);
				
				if(!spec_ty){	
					EM_error(d->pos, "vardec undefined type %s", S_name(d->u.var.typ));
					spec_ty = Ty_Int();
				}
				Ty_ty spec = actual_ty(spec_ty);

				//if record type we should check the addr
				if(spec->kind == Ty_record){ 
					if(init_exp.ty->kind != Ty_nil && spec != init_exp.ty){
						EM_error(d->pos, "type mismatch");
					}					
				}

				//not the recorf type
				else if (spec->kind != init_exp.ty->kind || (spec->kind == Ty_array && spec != init_exp.ty)) {
					EM_error(d->pos, "type mismatch");
				}
				S_enter(venv, d->u.var.var, E_VarEntry(access,spec));
			} 
			
			else {
				if (init_exp.ty->kind == Ty_nil) {
					EM_error(d->pos, "init should not be nil without type specified ");
				}
				S_enter(venv, d->u.var.var, E_VarEntry(access,init_exp.ty));
			}

			return Tr_Assign(Tr_simpleVar(access, level), init_exp.exp);
			
			break;
		}

		case A_functionDec:
		{
			A_fundecList fun;
			Ty_ty resultTy;
			Ty_tyList formalTys;
			U_boolList formalBools;			
			struct expty exp;
			for (fun = d->u.function; fun != NULL; fun = fun->tail) {

				//EM_error(d->pos,"fun %s",S_name(fun->head->result));
				if (fun->head->result != S_Symbol("")) {

					resultTy = S_look(tenv, fun->head->result);
					if(resultTy == NULL){	
						EM_error(d->pos, "undefined type %s", S_name(fun->head->result));
						resultTy = Ty_Int();
					}
				} 
				else {
					resultTy = Ty_Void();
				}
				formalTys = makeFormalTyList(d->pos,tenv, fun->head->params);

				//look up if there are functions with same name
				for(A_fundecList fl = fun->tail; fl != NULL; fl = fl->tail){
					if(fl->head->name == fun->head->name){
						EM_error(d->pos, "two functions have the same name");
					}
				}
				//EM_error(d->pos,"enter func %s",S_name(fun->head->name));
				
				//get parameters of this function into stack
				formalBools = makeFormalBoolList(fun->head->params);
				
				//S_enter(venv, fun->head->name, E_FunEntry(NULL,NULL,formalTys, resultTy));
				Temp_label funcName = Temp_newlabel();
				S_enter(venv, fun->head->name, E_FunEntry(Tr_newLevel(level, funcName, formalBools), funcName, formalTys, resultTy));				

			}

			E_enventry f;
			Ty_tyList t;
			A_fieldList l;
			for (fun = d->u.function; fun; fun = fun->tail) {
				S_beginScope(venv);
				f = S_look(venv, fun->head->name);
				//t = f->u.fun.formals;
				resultTy = f->u.fun.result;
				Tr_accessList params = Tr_formals(f->u.fun.level);
				for (l = fun->head->params,t = f->u.fun.formals; l != NULL; l = l->tail, t = t->tail) {
					fprintf(stdout,"enter param %s\n",S_name(l->head->name));
					S_enter(venv, l->head->name, E_VarEntry(params->head, t->head));



					///!!!!!!
					params=params->tail;					
				}
				exp = transExp(venv, tenv, fun->head->body, f->u.fun.level, NULL);
				if (exp.ty->kind != resultTy->kind) {
					if (resultTy->kind == Ty_void) {
						EM_error(fun->head->body->pos, "procedure returns value");
					} 
					else {
						EM_error(fun->head->body->pos, "type mismatch");
					}
				}
				S_endScope(venv);
				Tr_Func(exp.exp,f->u.fun.level);				
			}
			return Tr_Nop();
			break;
		}

		case A_typeDec:
		{
			A_nametyList l;
			int last, cur;
			for (l = d->u.type; l != NULL; l = l->tail) {
				//check if there is var with same name
				for(A_nametyList nl = l->tail; nl != NULL; nl = nl->tail){
					if(nl->head->name == l->head->name){
						EM_error(d->pos, "two types have the same name");
					}
				}
				
				//EM_error(d->pos,"enter var %s",S_name(l->head->name));
				S_enter(tenv, l->head->name, Ty_Name(l->head->name, NULL));				
			}

			int name_num = 0;
			Ty_ty ty;
			
			for(l = d->u.type; l != NULL; l = l->tail){
				//ty = S_look(tenv, l->head->name);
				switch(l->head->ty->kind){
					case A_nameTy:
					{
						ty = S_look(tenv, l->head->name);
						if(ty == NULL){	
							EM_error(d->pos, "undefined type %s", S_name(l->head->name));
							ty = Ty_Int();
						}
						ty->u.name.sym = l->head->ty->u.name;
						name_num++;
						break;
					}

					case A_recordTy:
					{
						ty = S_look(tenv, l->head->name);
						//EM_error(d->pos,"record1 %s",S_name(l->head->name));
						if(ty == NULL){	
							EM_error(d->pos, "undefined type %s", S_name(l->head->name));
							ty = Ty_Int();
						}
						ty->kind = Ty_record;
						//EM_error(d->pos,"record2 %s",S_name(l->head->name));
						
						ty->u.record = makeTyFieldList(d->pos,tenv, l->head->ty->u.record);
						//EM_error(d->pos,"record3 %s",S_name(l->head->name));
						
						break;
					}

					case A_arrayTy:
					{
						ty = S_look(tenv, l->head->name);
						if(ty == NULL){	
							EM_error(d->pos, "undefined type %s", S_name(l->head->name));
							ty = Ty_Int();
						}
						ty->kind = Ty_array;
						ty->u.array = S_look(tenv, l->head->ty->u.array);
						if(ty->u.array == NULL){
							EM_error(d->pos, "undefined type %s", S_name(l->head->ty->u.array));
							ty = Ty_Int();
						}
						break;
					}
				}				
				
			}


			//EM_error(d->pos,"name num %d",name_num);
			//check the iterated name type 
			while (name_num > 0){
				int temp_num = name_num;
				for(l = d->u.type; l != NULL ; l = l->tail){
					if(l->head->ty->kind == A_nameTy){
						ty = S_look(tenv, l->head->name);
						if(ty == NULL){	
							EM_error(d->pos, "undefined type %s", S_name(l->head->name));
							ty = Ty_Int();
						}

						if(ty->u.name.ty == NULL){
							Ty_ty nty = S_look(tenv, ty->u.name.sym);
							if(nty == NULL){	
								EM_error(d->pos, "undefined type %s", S_name(ty->u.name.sym));
								nty = Ty_Int();
							}
							if(nty->kind == Ty_name){
								if(nty->u.name.ty != NULL){
									ty->u.name.ty = nty->u.name.ty;
									name_num--;
								}
							}
							else{
								ty->u.name.ty = nty;
								name_num--;
							}
						}	
					}
					//EM_error(d->pos, "%d",name_num);
				}
				if(temp_num == name_num){
					EM_error(d->pos, "illegal type cycle");
					break;
				}

			}

			
			for (l = d->u.type; l != NULL; l = l->tail) {
				//i++;
				//EM_error(d->pos,"record5 %d",i);
				
				switch(l->head->ty->kind) {
					case A_nameTy:
						break;
					case A_recordTy:
					{
						//EM_error(d->pos,"record4 %s",S_name(l->head->name));
						
						ty = S_look(tenv, l->head->name);
						if(ty == NULL){	
							EM_error(d->pos, "undefined type %s", S_name(l->head->name));
							ty = Ty_Int();
						}
						
						//EM_error(d->pos,"record %s",S_name(l->head->name));						
						ty->u.record = actual_tys(ty->u.record);
						//EM_error(d->pos,"record %s",S_name(l->head->name));	
						
						break;
					}
					case A_arrayTy:
					{
						ty = S_look(tenv,l->head->name);
						if(ty == NULL){	
							EM_error(d->pos, "undefined type %s", S_name(l->head->name));
							ty = Ty_Int();
						}
						//EM_error(d->pos,"array %s",S_name(l->head->name));
						
						ty->u.array = actual_ty(ty->u.array);
						break;
					}
				}

				//i++;
				//EM_error(d->pos,"record5 %d",i);
			}

			///EM_error(d->pos,"record4 s");
			return Tr_Nop();
			break;
		}
	}
}



F_fragList SEM_transProg(A_exp exp)
{
	/*
	S_table venv = E_base_venv();
	S_enter(venv, S_Symbol("print"), E_FunEntry(Ty_TyList(Ty_String(), NULL), Ty_Void()));
	S_enter(venv, S_Symbol("flush"), E_FunEntry(NULL, Ty_Void()));
	S_enter(venv, S_Symbol("getchar"), E_FunEntry(NULL, Ty_String()));
	S_enter(venv, S_Symbol("ord"), E_FunEntry(Ty_TyList(Ty_String(), NULL), Ty_Int()));
	S_enter(venv, S_Symbol("chr"), E_FunEntry(Ty_TyList(Ty_Int(), NULL), Ty_String()));
	S_enter(venv, S_Symbol("size"), E_FunEntry(Ty_TyList(Ty_String(), NULL), Ty_Int()));
	S_enter(venv, S_Symbol("substring"), E_FunEntry(Ty_TyList(Ty_String(), Ty_TyList(Ty_Int(), Ty_TyList(Ty_Int(), NULL))), Ty_String()));
	S_enter(venv, S_Symbol("concat"), E_FunEntry(Ty_TyList(Ty_String(), Ty_TyList(Ty_String(), NULL)), Ty_String()));
	S_enter(venv, S_Symbol("not"), E_FunEntry(Ty_TyList(Ty_Int(), NULL), Ty_Int()));
	S_enter(venv, S_Symbol("exit"), E_FunEntry(Ty_TyList(Ty_Int(), NULL), Ty_Void()));
	*/
	Tr_level main=Tr_outermost();
    struct expty e = transExp(E_base_venv(), E_base_tenv(),exp,main,NULL);
    Tr_Func(e.exp,main);
    return Tr_getResult();

	//transExp(E_base_venv(), E_base_tenv(), exp);
}