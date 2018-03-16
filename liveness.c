#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "liveness.h"
#include "table.h"

static Temp_tempList MachineRegs;

bool ifMachineReg(Temp_temp a){
    if(a==F_eax()) return TRUE;
    if(a==F_ebx()) return TRUE;
    if(a==F_ecx()) return TRUE;
    if(a==F_edx()) return TRUE;
    if(a==F_esi()) return TRUE;
    if(a==F_edi()) return TRUE;
    return FALSE;
}



Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
	Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
	lm->src = src;
	lm->dst = dst;
	lm->tail = tail;
	return lm;
}


Temp_temp Live_gtemp(G_node n) {
	//your code here.
    return G_nodeInfo(n);
}

static G_node get_node(G_graph g, Temp_temp temp, TAB_table temp2node)
{
    assert(temp2node && temp);
    G_node res = TAB_look(temp2node, temp);
    if (res == NULL) {
        res = G_Node(g, temp);
        TAB_enter(temp2node, temp, res);
    }
    return res;
}


static void addConflict(struct Live_graph *g, Temp_temp temp_a, Temp_temp temp_b, TAB_table temp2node)
{
    if (temp_a == temp_b) return; 
    if (temp_a == F_FP() || temp_b == F_FP()) return; //ebp cannot coalesce so add no conflict 

    G_node a = get_node(g->graph, temp_a, temp2node);
    G_node b = get_node(g->graph, temp_b, temp2node);


    // 为了节省空间，这里不插入预着色寄存器的out边
    if (!ifMachineReg(temp_a) && temp_a!=F_FP()){
        assert(b);
        G_addEdge(a, b);
    }
    if (!ifMachineReg(temp_b) && temp_b!=F_FP()){
        assert(a);
        G_addEdge(b, a);
    }

}


struct Live_graph Live_liveness(G_graph flow) {
    //your code here.
    
    MachineRegs = Temp_TempList(F_eax(),
                  Temp_TempList(F_ebx(),
                  Temp_TempList(F_ecx(),
                  Temp_TempList(F_edx(),
                  Temp_TempList(F_esi(),
                  Temp_TempList(F_edi(), NULL))))));
	struct Live_graph lg;

	G_table in = G_empty();
	G_table out = G_empty();
	G_nodeList p = G_nodes(flow);
	
	//initialize the node list
	for (; p != NULL; p = p->tail) {
        G_enter(in, p->head, checked_malloc(sizeof(Temp_tempList*)));
        G_enter(out, p->head, checked_malloc(sizeof(Temp_tempList*)));
	}

	p = G_nodes(flow);
    bool loop = TRUE;
    
    //init in and out liveness
	while(loop){
		loop = FALSE;
		for(p = G_nodes(flow);p!=NULL;p = p->tail){
			Temp_tempList prevInp = *(Temp_tempList*)G_look(in, p->head);
            Temp_tempList prevOutp = *(Temp_tempList*)G_look(out, p->head);
            Temp_tempList inp; 
			Temp_tempList outp = NULL;
			for(G_nodeList succ = G_succ(p->head);succ != NULL;succ=succ->tail){
				Temp_tempList ins =*(Temp_tempList *)G_look(in, succ->head);
				outp = UnionList(outp, ins);
			}
            inp = UnionList(FG_use(p->head), MinusList(outp, FG_def(p->head)));
            
            //loop until in out not change
			if(!EqualList(inp,prevInp)){
				loop = TRUE;
				*(Temp_tempList*)G_look(in, p->head) = inp;   //
			}
			if(!EqualList(outp,prevOutp)){
				loop = TRUE;
				*(Temp_tempList *)G_look(out,p->head) = outp;
			}
		}
	}

	//confilct graph

    TAB_table temp2node = TAB_empty();
    lg.graph=G_Graph();
    lg.spillrate=G_empty();
    lg.moves=NULL;
    
    
	for (Temp_tempList m = MachineRegs; m != NULL; m = m->tail) {

        //modify
        //get_node(lg.graph, m->head, temp2node);
        int * r = checked_malloc(sizeof(int));
        *r = 0;
        G_node n = get_node(lg.graph, m->head, temp2node);
        //fprintf(stdout,"add rate %d\n",((Temp_temp)G_nodeInfo(n))->num);                
        G_enter(lg.spillrate,n,r);

    }
	
    for (p = G_nodes(flow); p != NULL; p = p->tail) {

		// add all the var of each instruction to the liveness graph
        for (Temp_tempList def = FG_def(p->head); def != NULL; def = def->tail) {
            if (def->head != F_FP()) {
                int * r = checked_malloc(sizeof(int));
                *r = 0;
                G_node n = get_node(lg.graph, def->head, temp2node);
                //fprintf(stdout,"add rate %d\n",((Temp_temp)G_nodeInfo(n))->num);                
                G_enter(lg.spillrate,n,r);
            }
		}
		
	}
    
    //machine reg confilct with each other
	for (Temp_tempList m1 = MachineRegs; m1; m1 = m1->tail) {
        for (Temp_tempList m2 = MachineRegs; m2; m2 = m2->tail) {
            if (m1->head != m2->head) {
                addConflict(&lg, m1->head, m2->head, temp2node);
            }
        }
    }

    //deal with each instruction
    int an=0;
    for (p = G_nodes(flow); p; p = p->tail) {
        an++;
        Temp_tempList outp = *(Temp_tempList*)G_look(out, p->head), op;
        AS_instr inst = G_nodeInfo(p->head);
        string a="mov `d0,`s0\n";
        //printf("inst %d\n",an);
        // if move a->b
        if (inst->kind == I_MOVE){//} && strcmp(a,inst->u.MOVE.assem)==0) {

            // Do not add the edge between these two vertexes
            outp = MinusList(outp, FG_use(p->head));
            for (Temp_tempList def = FG_def(p->head); def; def = def->tail) {
                for (Temp_tempList use = FG_use(p->head); use; use = use->tail) {
                    //modify
                    //ebp cannot coaleacse with other regs
                    if(use->head==F_FP()||def->head==F_FP()) {
                        fprintf(stdout,"att\n");
                        continue;
                    }
                    lg.moves = Live_MoveList(get_node(lg.graph, use->head, temp2node),
                            get_node(lg.graph, def->head, temp2node),
                            lg.moves);
                }
            }
        }

        // add conflict vertex
        for (Temp_tempList def = FG_def(p->head); def; def = def->tail) {
            for (op = outp; op; op = op->tail) {
                //addConflict(&lg, def->head, op->head, temp2node);
                //if(op->head==F_FP()||def->head==F_FP())  continue;
                //if(G_goesTo(get_node(lg.graph,def->head,temp2node),get_node(lg.graph,op->head,temp2node))||def->head==op->head) continue;                
                //fprintf(stdout,"start addConflict\n");
                addConflict(&lg, def->head, op->head, temp2node);
                //fprintf(stdout,"addConflict %d %d\n",def->head->num,op->head->num);
                if(op->head==F_FP()||def->head==F_FP())  continue;
                if(def->head==op->head) continue;                

                if (!ifMachineReg(def->head) && def->head!=F_FP()) {
                    int *r = G_look(lg.spillrate, get_node(lg.graph,def->head,temp2node));
                    //fprintf(stdout,"add spillrate to node %d currete %d\n",def->head->num,*r);                    
                    (*r)++;
                }
                if (!ifMachineReg(op->head) && op->head != F_FP()) {
                    int *r = G_look(lg.spillrate, get_node(lg.graph,op->head,temp2node));
                    //fprintf(stdout,"add spillrate to node %d currete %d\n",op->head->num,*r);                  
                    (*r)++;
                }
                //fprintf(stdout,"finish addConflict\n");
                
            }
        }
    }

	return lg;
}


