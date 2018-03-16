#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "liveness.h"
#include "color.h"
#include "regalloc.h"
#include "table.h"
#include "flowgraph.h"
#include "string.h"

#define K 6

#define MAXLEN 30
#define MAX 0xffffffff

static bool *adjSet;
static G_graph graph;
static G_table degree;
static G_table color;
static G_table alias;
static G_table spillrate;
static G_nodeList spillWorklist;
static G_nodeList simplifyWorklist;
static G_nodeList selectStack;
static G_nodeList spillNodes;
static G_nodeList coalescedNodes;
static G_nodeList freezeWorklist;
static Live_moveList worklistMoves;
static Live_moveList activeMoves;
static Live_moveList constrainedMoves;
static Live_moveList coalescedMoves;
static Live_moveList frozenMoves;


static int nodeNum = 0;

FILE* mout;// = fopen("log", "w");


static bool precolored(G_node n){
    assert(color && n);
    int *col = G_look(color, n);
    if(*col==0) return FALSE;
    else return TRUE;
}


bool* getAdjOffset(bool* start,int size,int i,int j){
	return start+(i*size+j);
}

bool Live_ExistInList(G_node src, G_node dst, Live_moveList l)
{
    for (Live_moveList p = l; p; p = p->tail) {
        if (p->src == src && p->dst == dst) {
            return TRUE;
        }
    }
    return FALSE;
}

Live_moveList Live_UnionList(Live_moveList l, Live_moveList r)
{
    Live_moveList res = r;
    for (Live_moveList p = l; p; p = p->tail) {
        if (!Live_ExistInList(p->src, p->dst, r)) {
            res = Live_MoveList(p->src, p->dst, res);
        }
    }
    return res;
}

Live_moveList Live_MinusList(Live_moveList l, Live_moveList r)
{
    Live_moveList res = NULL;
    for (Live_moveList p = l; p; p = p->tail) {
        if (!Live_ExistInList(p->src, p->dst, r)) {
            res = Live_MoveList(p->src, p->dst, res);
        }
    }
    return res;
}

static void build(struct Live_graph g)
{
    degree = G_empty();
    color = G_empty();
    alias = G_empty();
    spillrate = G_empty();
	nodeNum=0;
	for (G_nodeList p = G_nodes(g.graph); p; p = p->tail) {
		nodeNum++;
	}	
    adjSet=checked_malloc(nodeNum*nodeNum*sizeof(bool));
    
    //!!!!waning
    memset(adjSet,0,nodeNum*nodeNum);

    fprintf(mout,"[info]node num: %d\n",nodeNum);

    G_nodeList p;
    int am=0;
    for ( p = G_nodes(g.graph); p; p = p->tail) {
        int index1 = G_getKey(p->head);
        int * t = checked_malloc(sizeof(int));
        *t = 0;
        G_nodeList cur;
        //modify
		for (cur = G_succ(p->head); cur; cur = cur->tail) {
            int index2 = G_getKey(cur->head);            
            bool* index = getAdjOffset(adjSet,nodeNum,index1,index2);
            if(*index) continue;
            *index=TRUE;
            ++(*t);
            //fprintf(stdout,"edge %d-%d\n",Live_gtemp(p->head)->num,Live_gtemp(cur->head)->num);
			//index2 = G_getKey(cur->head);
			//index = getAdjOffset(adjSet,nodeNum,index2,index1);
			//*index = TRUE;
		}
        G_enter(degree, p->head, t);

        int * c = checked_malloc(sizeof(int));
        Temp_temp temp = Live_gtemp(p->head);
        /*
        if(temp==F_FP()) {
            fprintf(stdout,"ebp");
            exit(0);
        }
        */
        if (temp == F_eax()) {
            *c = 1;
        } 
        else if (temp == F_ebx()) {
            *c = 2;
        } 
        else if (temp == F_ecx()) {
            *c = 3;
        } 
        else if (temp == F_edx()) {
            *c = 4;
        } 
        else if (temp == F_esi()) {
            *c = 5;
        } 
        else if (temp == F_edi()) {
            *c = 6;
        } 
        else {
            *c = 0;
        }
        G_enter(color, p->head, c);

        G_node * a = checked_malloc(sizeof(G_node));
        *a = p->head;
        G_enter(alias, p->head, a);
    }
    
    for ( p = G_nodes(g.graph); p; p = p->tail) {
        am++;
        int *tn=G_look(degree,p->head);
        int inf = Live_gtemp(p->head)->num;

        int *sr=G_look(g.spillrate,p->head);
        //if(precolored(p->head)) fprintf(stdout,"colored \n");
        //fprintf(stdout,"degree %d tmpnum %d :%d spill rate %d\n",am,inf,*tn,*sr);
        fprintf(mout,"degree %d tmpnum %d :%d spill rate %d\n",am,inf,*tn,*sr);
        
    }

    am=0;
    for (Live_moveList cur = g.moves; cur; cur = cur->tail) {
        am++;
        fprintf(mout,"move instruction %d src %d dst %d\n",am,Live_gtemp(cur->src)->num,Live_gtemp(cur->dst)->num);
    }

    for(int i=0;i<nodeNum;i++){
        for(int j=0;j<nodeNum;j++){
            //fprintf(mout,"%d ",(int)*getAdjOffset(adjSet,nodeNum,i,j));
        }
        //fprintf(mout,"\n");
    }
   
    graph = g.graph;
    worklistMoves = g.moves;    
    spillrate= g.spillrate;
    spillWorklist = NULL;
    simplifyWorklist = NULL;
    freezeWorklist = NULL;
    activeMoves = NULL;
    frozenMoves = NULL;
    constrainedMoves = NULL;
    coalescedMoves = NULL;
    selectStack = NULL;
    coalescedNodes = NULL;
}


static G_node GetAlias(G_node n)
{
    //assert(alias && n);
    if(G_inNodeList(n,coalescedNodes)){
        G_node* a=G_look(alias,n);
        return GetAlias(*a);
    }
    else return n;
    /*
    G_node * a = G_look(alias, n);
    //modify
    if (G_inNodeList(a,coalescedNodes)) {
        *a = GetAlias(*a);
    }
    return *a;
    */
}


static Live_moveList nodeMoves(G_node n)
{
    Live_moveList p = Live_UnionList(activeMoves, worklistMoves);
    Live_moveList res = NULL;
    G_node m = GetAlias(n);


    //??????
    //and adjlist and (actionlist union worklist)
    for (Live_moveList cur = p; cur != NULL; cur = cur->tail) {
        //if (GetAlias(cur->src) == m || GetAlias(cur->dst) == m) {            
        if (GetAlias(cur->src) == m || GetAlias(cur->dst) == m || cur->src == m || cur->dst ==m) {
            res = Live_MoveList(cur->src, cur->dst, res);
        }
    }
    return res;
}

static bool moveRelated(G_node n){
    Live_moveList res = nodeMoves(n);
    if(res != NULL) return TRUE;
    else return FALSE;
}

static void makeWorklist()
{
    G_nodeList p = G_nodes(graph);
    for (; p; p = p->tail) {
        assert(degree && p->head);
        int* deg = G_look(degree, p->head);
        assert(color && p->head);
        int* c = G_look(color, p->head);
        if (*c == 0) {
            if (*deg >= K) {
                fprintf(mout,"add potential spill node tempnum %d degree %d\n",Live_gtemp(p->head)->num,*deg);
                spillWorklist = G_NodeList(p->head, spillWorklist);
            } 
            else if (moveRelated(p->head)) {
                freezeWorklist = G_NodeList(p->head, freezeWorklist);
            } 
            else {
                simplifyWorklist = G_NodeList(p->head, simplifyWorklist);
            }
        }
    }
}

static G_nodeList adjacent(G_node n)
{
    //???
    return G_MinusList(G_succ(n),G_UnionList(selectStack, coalescedNodes));
    
}

static void enableMoves(G_nodeList nodes)
{
    for (G_nodeList p = nodes; p != NULL; p = p->tail) {
        for (Live_moveList m = nodeMoves(p->head); m; m = m->tail) {
            //fprintf(stdout,"enable %d\n",Live_gtemp(p->head)->num);
            if (Live_ExistInList(m->src, m->dst, activeMoves)) {
                activeMoves = Live_MinusList(activeMoves, Live_MoveList(m->src, m->dst, NULL));
                worklistMoves = Live_UnionList(worklistMoves, Live_MoveList(m->src, m->dst, NULL));
                fprintf(mout,"new worklist %d %d\n",Live_gtemp(m->src)->num,Live_gtemp(m->dst)->num);
            }
        }
    }
}


static void decrementDegree(G_node n){
    //fprintf(stdout,"decrementDegree node tempnum %d \n",Live_gtemp(n)->num);        
    
    assert(degree && n);
    int* d = G_look(degree,n);
    int deg = *d;
    //fprintf(stdout,"decrementDegree node tempnum %d cur degree %d\n",Live_gtemp(n)->num,deg);        
    
    //????
    *d = deg - 1;
    int *c = G_look(color, n);
    
    //???
    //ensure the node is in the spillworklist
    //fprintf(stdout,"try to decrement %d\n",Live_gtemp(n)->num);
    if(*d < K && *c == 0 && G_inNodeList(n, spillWorklist)){
        enableMoves(G_NodeList(n, adjacent(n)));
        fprintf(mout,"delete spill node %d\n",Live_gtemp(n)->num);
        spillWorklist = G_MinusList(spillWorklist,G_NodeList(n,NULL));
        if (moveRelated(n)) {
            freezeWorklist = G_NodeList(n, freezeWorklist);
        } 
        else {
            //fprintf(stdout,"add simplify %d  decre\n",Live_gtemp(n)->num);
            simplifyWorklist = G_NodeList(n, simplifyWorklist);
        }        
    }
}

static void simplify(){
    //choose the node to simpify from the head 
    G_node cur = simplifyWorklist->head;
    simplifyWorklist = simplifyWorklist->tail;
    selectStack = G_NodeList(cur, selectStack);  
    //fprintf(stdout,"simplify node %d\n",Live_gtemp(cur)->num);        
    fprintf(mout,"simplify node %d\n",Live_gtemp(cur)->num);        
    
    for(G_nodeList adjlist = adjacent(cur);adjlist!=NULL;adjlist=adjlist->tail){
        //fprintf(stdout,"adjlist %d\n",Live_gtemp(adjlist->head)->num);        
        if(precolored(adjlist->head)) continue;
        decrementDegree(adjlist->head);
        //fprintf(stdout,"simplify %d-%d\n",Live_gtemp(cur)->num,Live_gtemp(adjlist->head)->num);
    }

}



static bool OK(G_node v, G_node u){
    assert(degree  && v);
    int * deg = G_look(degree, v);
    bool *flag = getAdjOffset(adjSet, nodeNum, G_getKey(v), G_getKey(u));
    if (!precolored(v) && *deg >= K && !(*flag)) {
    //if (!precolored(v) && *deg >= K && !G_goesTo(v,u)) {
            
         return FALSE;
    }
    return TRUE;
}



static void addWorkList(G_node u){
    assert(degree && u);
    int * deg = G_look(degree, u);
    if (!precolored(u) && !moveRelated(u) && *deg < K) {
        freezeWorklist = G_MinusList(freezeWorklist, G_NodeList(u, NULL));
        //fprintf(stdout,"add simplify %d  adddwork\n",Live_gtemp(u)->num);
        simplifyWorklist = G_NodeList(u, simplifyWorklist);
    }
}

static bool conservative(G_nodeList nodes){
    int k  = 0;
    for(G_nodeList l=nodes;l != NULL;l=l->tail){
        assert(degree && l->head);
        if(*(int*)G_look(degree,l->head)>=K || precolored(l->head)){
            k++;
        }
    }
    return (k<K);
}

static void addEdge(G_node u, G_node v){
    bool * flag = getAdjOffset(adjSet, nodeNum, G_getKey(u), G_getKey(v));
    if (u != v && *flag == FALSE) {
        *flag = TRUE;
        flag = getAdjOffset(adjSet, nodeNum, G_getKey(v), G_getKey(u));
        *flag = TRUE;
        if (!precolored(u)&&Live_gtemp(u)!=F_FP()) {
            assert(degree  && u);
            if(!G_goesTo(u,v)){
            int * deg = G_look(degree, u);
            ++(*deg);
            assert(v);
            //fprintf(stdout,"add %d degree with edge %d\n",Live_gtemp(u)->num,Live_gtemp(v)->num);
            G_addEdge(u, v);
            }
        }
        if (!precolored(v)&&Live_gtemp(v)!=F_FP()) {
            assert(degree && v);
            if(!G_goesTo(v,u)){
            int * deg = G_look(degree, v);
            ++(*deg);
            assert(v);
            //fprintf(stdout,"add %d degree with edge %d\n",Live_gtemp(v)->num,Live_gtemp(u)->num);            
            G_addEdge(v, u);
            }
        }
    }
}


static void combine(G_node u, G_node v){
    //fprintf(stdout,"combine %d %d\n",((Temp_temp)Live_gtemp(u)->num),((Temp_temp)Live_gtemp(v)->num));
    if(G_inNodeList(v,freezeWorklist)){
        freezeWorklist = G_MinusList(freezeWorklist, G_NodeList(v, NULL));        
    }
    else{
        //bug2
        int* d=G_look(degree,v);

        //check the degree before delete from spillworklist
        if(*d<K){
        fprintf(mout,"a delete spill node %d\n",Live_gtemp(v)->num);
        spillWorklist = G_MinusList(spillWorklist, G_NodeList(v, NULL));
        }
    }
    coalescedNodes = G_NodeList(v,coalescedNodes);
    assert(alias && v);
    *(G_node*)G_look(alias, v) = u;
    enableMoves(G_NodeList(v,NULL)); ///???

    int anum=0;
    for(G_nodeList lst=G_succ(v);lst;lst=lst->tail){
        anum++;
    }
    fprintf(mout,"node %d has %d succ\n",Live_gtemp(v)->num,anum);

    for (G_nodeList t = adjacent(v); t != NULL; t = t->tail) {
        //fprintf(stdout,"combine %d,%d\n",Live_gtemp(u).num,Live_gtemp(v).num);
        //if(G_goesTo(t->head,u)) continue;        
        addEdge(t->head, u);
        //fprintf(stdout,"combine add edge %d %d\n",Live_gtemp(t->head)->num,Live_gtemp(u)->num);
        //fprintf(mout,"decrementDegree node tempnum %d \n",Live_gtemp(t->head)->num);        
        if(precolored(t->head)) continue;
        decrementDegree(t->head);
        fprintf(mout,"decrementDegree node tempnum %d \n",Live_gtemp(t->head)->num);        
        
    }
 
    assert(degree && u);
    if (*(int*)G_look(degree, u) >= K && G_inNodeList(u, freezeWorklist)) {
        freezeWorklist = G_MinusList(freezeWorklist, G_NodeList(u, NULL));        
        spillWorklist = G_NodeList(u, spillWorklist);
    }
}   



static void coalesce(){
    G_node u, v;
    G_node x = worklistMoves->src;
    G_node y = worklistMoves->dst;
    //fprintf(mout,"coalesce %d %d\n",Live_gtemp(x)->num,Live_gtemp(y)->num);
    
    if (precolored(GetAlias(y))) {
        u = GetAlias(y);
        v = GetAlias(x);
    } else {
        u = GetAlias(x);
        v = GetAlias(y);
    }
    //fprintf(stdout,"coalesce %d %d\n",Live_gtemp(x)->num,Live_gtemp(y)->num);
    fprintf(mout,"coalesce %d %d\n",Live_gtemp(x)->num,Live_gtemp(y)->num);
    
    int anum=0;
    Live_moveList test=worklistMoves;
    while(test){
        anum++;
        test=test->tail;
    }
    fprintf(mout,"worklist num %d\n",anum);

    anum = 0;
    for(G_nodeList lst=G_succ(v);lst;lst=lst->tail){
        fprintf(mout,"node %d has succ %d\n",Live_gtemp(v)->num,Live_gtemp(lst->head)->num);
    }

    fprintf(mout,"edge between %d %d exist? %d\n",Live_gtemp(u)->num,Live_gtemp(v)->num,(int)*getAdjOffset(adjSet,nodeNum,G_getKey(u),G_getKey(v)));

    worklistMoves = worklistMoves->tail;
    if(u==v){
        coalescedMoves = Live_MoveList(x,y,coalescedMoves);
        addWorkList(u);
    }

    //I don't know why???? there must use twice check because only one side will cause false???
    else if(precolored(v)  || (*getAdjOffset(adjSet,nodeNum,G_getKey(v),G_getKey(u))||*getAdjOffset(adjSet,nodeNum,G_getKey(u),G_getKey(v)))==TRUE ){
    //else if(precolored(v)  || G_goesTo(u,v) ){
            
        constrainedMoves = Live_MoveList(x,y,constrainedMoves);
        addWorkList(u);
        addWorkList(v);
    }
    else {
        
        G_nodeList t = adjacent(v);
        bool flag = TRUE;
        for(;t != NULL; t=t->tail){
            if(!OK(t->head,u)){
                flag=FALSE;
                break;
            }
        }
        
        //modify
        fprintf(mout,"flag %d\n",(int)flag);
        if((precolored(u) &&  flag)  || (!precolored(u) && conservative(G_UnionList(adjacent(u),adjacent(v))))){                
            coalescedMoves  = Live_MoveList(x,y,coalescedMoves);
            combine(u,v);
            addWorkList(u);
        }
        else{
            //????
            activeMoves = Live_MoveList(x,y,activeMoves);
        }
    }
    

}

static void freezeMoves(G_node u){
    G_node v;
    for (Live_moveList m = nodeMoves(u); m; m = m->tail) {
        if (GetAlias(m->dst) == GetAlias(u)) {
            v = GetAlias(m->src);
        } 
        else {
            v = GetAlias(m->dst);
        }
        activeMoves = Live_MinusList(activeMoves, Live_MoveList(m->src, m->dst, NULL));
        frozenMoves = Live_UnionList(frozenMoves, Live_MoveList(m->src, m->dst, NULL));
        assert(degree && v);
        int *deg = G_look(degree, v);
        if (!moveRelated(v) && !precolored(v) && *deg < K) {
            freezeWorklist = G_MinusList(freezeWorklist, G_NodeList(v, NULL));
            //fprintf(stdout,"add simplify %d  freezemv\n",Live_gtemp(v)->num);            
            simplifyWorklist = G_NodeList(v, simplifyWorklist);
        }
    }
}


static void freeze(){
    G_node u = freezeWorklist->head;
    freezeWorklist = freezeWorklist->tail;
    //fprintf(stdout,"add simplify %d  freeze\n",Live_gtemp(u)->num);   
    simplifyWorklist = G_NodeList(u, simplifyWorklist);
    //fprintf(stdout,"freeze %d\n",Live_gtemp(u)->num);
    fprintf(mout,"freeze %d\n",Live_gtemp(u)->num);
    
    freezeMoves(u);
}


static void selectSpill(){
    G_node m = spillWorklist->head;
    assert(spillrate && m);
    assert(degree && m);
    int times = *(int *)G_look(spillrate, m);
    int deg = *(int*)G_look(degree,m);
    assert(deg>=K);

    //use (def+use)/deg
    double min=(double)times/deg; 

    for (G_nodeList p = spillWorklist->tail; p!= NULL; p = p->tail) {
        assert(spillrate&&p->head);
        assert(degree && m);        
        times = *(int *)G_look(spillrate, p->head);
        deg = *(int*)G_look(degree,m);
        double t = times/deg;
        if (Live_gtemp(p->head)->spilled) {
            t = MAX; // spilled register has a lower priority to be spilled again 
        }
        if (t < min) {
            min = t;
            m = p->head;
        }
    }

    //fprintf(stdout,"spill nodenum %d\n",G_getKey(m));
    fprintf(mout,"spill nodenum %d\n",Live_gtemp(m)->num);
    
    spillWorklist = G_MinusList(spillWorklist, G_NodeList(m, NULL));
    simplifyWorklist = G_NodeList(m, simplifyWorklist);
    freezeMoves(m);
}

static void assignColors(){
    bool okColor[K+1];
    //m1
    int i;
    spillNodes=NULL;
    while(selectStack){
        //fprintf(stdout,"5\n");
        //fprintf(mout,"5\n");
        
        G_node n = selectStack->head;
        selectStack = selectStack->tail;
        for (i = 1; i <= K; ++i) {
            okColor[i] = TRUE;
        }
        for (G_nodeList p = G_succ(n); p; p = p->tail) {
            assert(color &&  GetAlias(p->head));
            int *t = G_look(color, GetAlias(p->head));
            /* printf("color of %d is %d\n", Live_gtemp(p->head)->num, *t); */
            okColor[*t] = FALSE;
        }
        //int i;
        for(i=1;i<=K;++i){
            if(okColor[i]){
                break;
            }
        }
        ///?? num==K
        //m1
        //if(num > K){
        if(i > K){
            spillNodes = G_NodeList(n, spillNodes);          
        }
        else{
            assert(color && n);
            *(int*)G_look(color, n)=i;
        }
    }

    ///???
    for (G_nodeList p = coalescedNodes; p != NULL; p = p->tail) {
        assert(color && GetAlias(p->head));
        assert(color && p->head);
        int *t = G_look(color, GetAlias(p->head));
        int *c = G_look(color, p->head);
        *c = *t;
    }
}

Temp_tempList* Inst_def(AS_instr inst) {
    switch (inst->kind) {
        case I_OPER:
            return &inst->u.OPER.dst;
        case I_LABEL:
            return NULL;
        case I_MOVE:
            return &inst->u.MOVE.dst;
        default:
            assert(0);
    }
    return NULL;
}

Temp_tempList* Inst_use(AS_instr inst) {
    switch (inst->kind) {
        case I_OPER:
            return &inst->u.OPER.src;
        case I_LABEL:
            return NULL;
        case I_MOVE:
            return &inst->u.MOVE.src;
        default:
            assert(0);
    }
    return NULL;
}

Temp_tempList Temp_replaceTempList(Temp_tempList l, Temp_temp old, Temp_temp new){
    if (l) {
        if (l->head == old) {
            return Temp_TempList(new, Temp_replaceTempList(l->tail, old, new));
        } else {
            return Temp_TempList(l->head, Temp_replaceTempList(l->tail, old, new));
        }
    } else {
        return NULL;
    }
}


static void rewriteProgram(F_frame f, AS_instrList *pil) {
    AS_instrList initial = *pil, l, last, next, new_instr;
    int off;
    while(spillNodes) {
        
        G_node cur = spillNodes->head;
        spillNodes = spillNodes->tail;
        /* assert(!precolored(cur)); */
        Temp_temp c = Live_gtemp(cur);
        int *d=(int*)G_look(degree,cur);
        fprintf(mout,"cur spill %d degree  %d\n",c->num,*d);
        off = F_allocSpill(f);
        //fprintf(stdout,"off %d\n",off);
        l = initial;
        last = NULL;
        next = initial->tail;
        AS_instrList temp=NULL;
        int a=0;
        while(l){
            l=l->tail;
            a++;
        }
        //fprintf(stdout,"warning %d\n",a);        
        a=0;
        l = initial;
        while(l){
            a++;
            //fprintf(stdout,"7 %d\n",a);
            Temp_temp t=NULL;
            Temp_tempList *def = Inst_def(l->head);
            Temp_tempList *use = Inst_use(l->head);
            //fprintf(mout,"start instr\n");
            //AS_print(mout,l->head,Temp_layerMap(Temp_empty(), Temp_name()));
            
            if(use!=NULL && ExistInList(c,*use)){
                if(t==NULL){
                    t = Temp_newtemp();
                    t->spilled = TRUE;
                }
                fprintf(mout,"original instr:  ");
                AS_print(mout,l->head,Temp_layerMap(Temp_empty(), Temp_name()));
                
                *use = Temp_replaceTempList(*use,c,t);
                char *a=checked_malloc(MAXLEN*sizeof(char));
                //load before use
                sprintf(a,"movl %d(%%ebp), `d0\n",off);
                //fprintf(mout,AS_getAssem(l->head));
                fprintf(mout,a);
                AS_print(mout,l->head,Temp_layerMap(Temp_empty(), Temp_name()));
                
                new_instr = AS_InstrList(AS_Oper(a, Temp_TempList(t, NULL), NULL, AS_Targets(NULL)), l);
                if(last==NULL){
                    initial=new_instr;
                }
                else{
                    last->tail = new_instr;
                }
                last = new_instr;
                //AS_printInstrList(mout,new_instr,Temp_layerMap(Temp_empty(), Temp_name()));
                fprintf(mout,"end replace  instr1\n");

            }

            //last = l;
            if(def!=NULL && ExistInList(c,*def)){
                if(t==NULL){
                    t = Temp_newtemp();
                    t->spilled=TRUE;
                }
                fprintf(mout,"original instr:  ");
                AS_print(mout,l->head,Temp_layerMap(Temp_empty(), Temp_name()));

                *def = Temp_replaceTempList(*def,c,t);
                char *a=checked_malloc(MAXLEN*sizeof(char));
                // store after load
                sprintf(a,"movl `s0, %d(%%ebp)\n",off);
                fprintf(mout,a);
                //fprintf(mout,AS_getAssem(l->head));  
                AS_print(mout,l->head,Temp_layerMap(Temp_empty(), Temp_name()));
                
                new_instr = AS_InstrList(AS_Oper(a, NULL, Temp_TempList(t, NULL), AS_Targets(NULL)), next);
                l->tail = new_instr;
                //last = l->tail;
                l = new_instr;
                //AS_printInstrList(mout,new_instr,Temp_layerMap(Temp_empty(), Temp_name()));
                fprintf(mout,"end replace  instr2\n");
                
            }

            last = l;
            l =next;
            if(next==NULL) break;
            next=next->tail;

            //fprintf(mout,"end\n");
        }
    }
    *pil = initial;

    //bug
    coalescedNodes=NULL;
}

//map correspding register
static Temp_map generate_map(){
    Temp_map res = Temp_empty();
    G_nodeList p = G_nodes(graph);
    for (; p != NULL; p = p->tail) {
        assert(color && p->head);
        int *c = G_look(color, p->head);
        string assignReg;
        if(*c==1) assignReg="%eax";
        if(*c==2) assignReg="%ebx";
        if(*c==3) assignReg="%ecx";
        if(*c==4) assignReg="%edx";
        if(*c==5) assignReg="%esi";
        if(*c==6) assignReg="%edi";
        
        Temp_enter(res, Live_gtemp(p->head), assignReg);
        fprintf(mout,"assign node %d color %s\n",Live_gtemp(p->head)->num,assignReg);
    }

    /* ebp */
    Temp_enter(res, F_FP(), "%ebp");

    return res;
}


struct RA_result RA_regAlloc(F_frame f, AS_instrList il) {
    //your code here
    mout = fopen("loga", "w");
	struct Live_graph live_graph;
    bool done = FALSE;
    //spillWorklist = NULL;
    int round=0;
    while (!done) {
        G_graph flow_graph = FG_AssemFlowGraph(il, f);
        live_graph = Live_liveness(flow_graph);
		build(live_graph);		
        makeWorklist();
        
        while (simplifyWorklist || spillWorklist || worklistMoves || freezeWorklist) {
            if (simplifyWorklist) {
                //fprintf(stdout,"1\n");
                //fprintf(mout,"1\n");
                
                simplify();
            } 
            else if (worklistMoves) {
                //fprintf(stdout,"2\n");
                //fprintf(mout,"2\n");
                
                coalesce();
            } 
            else if (freezeWorklist) {
                //fprintf(stdout,"3\n");
                //fprintf(mout,"3\n");
                
                freeze();
            } 
            else if (spillWorklist) {
                //fprintf(stdout,"4\n");
                //fprintf(mout,"4\n");
                
                selectSpill();
            }
            int am=0;
            round++;
            //fprintf(stdout,"Round %d\n",round);
            fprintf(mout,"Round %d\n",round);
            
            //bug
            if(round>=500) {
                //fprintf(stdout,"%d\n",F_FP()->num);
                //exit(0);
            }
            
            for (G_nodeList p = G_nodes(live_graph.graph); p; p = p->tail) {
                am++;
                int *tn=G_look(degree,p->head);
                int inf = Live_gtemp(p->head)->num;                
                //fprintf(stdout,"degree %d tmpnum %d :%d\n",am,inf,*tn);
                fprintf(mout,"degree %d tmpnum %d :%d\n",am,inf,*tn);
                
            }
            

        }
        
        assignColors();
        
        if (spillNodes) {
            rewriteProgram(f, &il);
        } else {
            done = TRUE;
        }
        
		
	}
	

    struct RA_result ret;
    ret.il = il;
    ret.coloring = generate_map();
    AS_printInstrList (mout, il, Temp_layerMap(F_tempMap, ret.coloring));
    
    return ret;
}
