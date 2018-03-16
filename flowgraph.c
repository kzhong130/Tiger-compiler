#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "errormsg.h"
#include "table.h"

Temp_tempList FG_def(G_node n) {
	AS_instr inst = G_nodeInfo(n);
   
	if(inst->kind == I_OPER){
		return inst->u.OPER.dst;
	}
	if(inst->kind == I_MOVE){
		return inst->u.MOVE.dst;
	}
    return NULL;
}

Temp_tempList FG_use(G_node n) {
	AS_instr inst = G_nodeInfo(n);
  
	if(inst->kind == I_OPER){
		return inst->u.OPER.src;
	}
	if(inst->kind == I_MOVE){
		return inst->u.MOVE.src;
	}

    return NULL;
}

bool FG_isMove(G_node n) {
	AS_instr inst = G_nodeInfo(n);
	if(inst->kind == I_MOVE) return TRUE;
	else return FALSE;
}

G_graph FG_AssemFlowGraph(AS_instrList il, F_frame f) {
	TAB_table label = TAB_empty();
    TAB_table instr = TAB_empty();
    G_graph res = G_Graph();
	G_node last = NULL;
	
	// Add all the label sequentially to the graph first 
    for (AS_instrList cur = il; cur != NULL; cur = cur->tail) {
        G_node node = G_Node(res, cur->head);
        TAB_enter(instr, cur->head, node);
        if (last) {
            // add edge to last node
            assert(node);
            G_addEdge(last, node);
        }
		last = node;
		//add to the label table
        if (cur->head->kind == I_LABEL) {
            TAB_enter(label, cur->head->u.LABEL.label, node);
        }
    }
    for (AS_instrList cur = il; cur != NULL; cur = cur->tail) {

		//deal with jump and cjump
        if (cur->head->kind == I_OPER) {
            assert(instr && cur->head);
            G_node node = TAB_look(instr, cur->head);
            Temp_labelList l;
            if(!cur->head->u.OPER.jumps){
                l = NULL;
            }
            else l = cur->head->u.OPER.jumps->labels;
            for (; l != NULL; l = l->tail) {
                assert(TAB_look(label,l->head));
                G_addEdge(node, TAB_look(label, l->head));
            }
        }
    }
    return res;
}
