#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>

typedef void *MySemaphore;
typedef void *MyThread;
typedef struct Node{
	int id;
	ucontext_t thread;
	int *c_id;
	int noc;//no of children
	char status;
	int nob;//no of blockers
	int *blockers;
}Node;
typedef struct Queue{
	int size;
	int front;
	int rear;
	Node* *elements;
}Queue;
typedef struct Sem{
	int val;
	int id;
	char status;
	Queue* waiting;
}Sem;

Queue* ready;//ready queue
Node* *blocked;//list of blocked
Sem* *sems;//list of semaphores
int id;//id's for threads
int b_s;//size of blocked
int s_id;//id's for semaphores
ucontext_t ori;//context to return to(original)
ucontext_t exitt;
Node* curr;//currently running context

Queue * createQueue(){
	/* Create a Queue */
	Queue *Q;
	Q = (Queue *)malloc(sizeof(Queue));
	/* Initialise its properties */
	Q->elements = NULL;//(Node *)malloc(sizeof(Node)*1);
	Q->size = 0;
	Q->front = 0;
	Q->rear = -1;
	/* Return the pointer */
	return Q;
}
Node* Dequeue(Queue *Q){
	/* If Queue size is zero then it is empty. So we cannot pop */
	if(Q->size==0)
	{
		printf("Queue is Empty\n");
		return;
	}
	/* Removing an element*/
	else
	{
		Q->size--;
		Node *temp=(Q->elements[Q->front]); 
		Q->front++;
		return temp;
	}
}
int find(Queue *Q,int n){
	int i;
	for(i=Q->front;i<=Q->rear;i++)
		if(Q->elements[i]->id==n)
			return 1;
	return 0;
}
void Enqueue(Queue *Q,Node* element){
	Node **temp=(Node**)realloc(Q->elements,((Q->rear)+2)*sizeof(Node*));
	if(temp!=NULL)
		Q->elements=temp;
	else
	{
		printf("Error allocating memory for Thread\n");
		exit(-1);
	}
	Q->size++;
	Q->rear++;
	Q->elements[Q->rear] = element;
}

// ****** THREAD OPERATIONS ****** 
// Terminate invoking thread
void MyThreadExit(){
	//printf("Exiting %d\n",curr->id);
	int i,flag=0,j;
	//checking all blocked threads
	for(i=0;i<b_s && flag==0;i++){
		if(blocked[i]->status=='b'){
			flag=0;
			for(j=0;j<(blocked[i]->noc);j++)
				if(blocked[i]->c_id[j]==curr->id)
					flag=1;
			if(flag==1){ //if curr is child of the blocked
				int f=0;
				for(j=0;j<blocked[i]->nob;j++)
					if(blocked[i]->blockers[j]==curr->id){
						blocked[i]->blockers[j]=0;
						f=1;					
					}
				if(f==1){ //if curr is blocking the blocked
					f=0;
					for(j=0;j<blocked[i]->nob;j++)
						f+=blocked[i]->blockers[j];
					if(f==0){ //if all blocking threads have finished exec
						blocked[i]->status='r';
						blocked[i]->nob=0;
						free(blocked[i]->blockers);
						//printf("Unblocking %d\n",blocked[i]->id);
						Enqueue(ready,blocked[i]);
					}
				}
			}	
		}
	}
	
	ucontext_t temp;
	curr->thread.uc_link=&temp;
	
	if(ready->size>0){
		curr=Dequeue(ready);
		//printf("Starting %d\n",curr->id);
		setcontext(&(curr->thread));
	}
	else
		setcontext(&ori);
}

// Create a new thread.
MyThread MyThreadCreate(void(*start_funct)(void *), void *args){
	//printf("Creating %d\n",id);
	ucontext_t nt,temp;
	
	nt.uc_stack.ss_sp=malloc(8192);
	nt.uc_stack.ss_size=8192;
	nt.uc_link=&temp;
	makecontext(&nt,MyThreadExit,0);
	exitt=nt;
	Node *n=(Node *)malloc(sizeof(Node)*1);
	getcontext(&(n->thread));
	n->thread.uc_stack.ss_sp = malloc(8192);
  	n->thread.uc_stack.ss_size = 8192;
    n->thread.uc_link = &nt;
	n->id=id;
	n->status='r';
	n->noc=0;
	n->nob=0;
	if(curr!=NULL){
		curr->noc++;
		curr->c_id=realloc(curr->c_id,curr->noc*sizeof(int));
		curr->c_id[(curr->noc)-1]=n->id;
	}
	id++;
	makecontext(&(n->thread), (void (*)(void))start_funct, 1, args);
	Enqueue(ready,n);
	return (void *)n;
	//free(n);
}

// Yield invoking thread
void MyThreadYield(){
	//printf("Yielding %d\n",curr->id);
	if(ready->size>0){
		Node* n=curr;
		Enqueue(ready,n);
		curr=Dequeue(ready);
		//printf("Starting %d\n",curr->id);
		swapcontext(&(n->thread),&(curr->thread));
	}
}

// Join with a child thread
int MyThreadJoin(MyThread thread){
	Node* t=(Node *)thread;
	//printf("Joining %d to %d\n",curr->id,t->id);
	int i,flag=0;
	for(i=0;i<(curr->noc);i++)
		if(curr->c_id[i]==t->id)
			flag=1;
	if(flag==1){
		if(find(ready,t->id)==1){
			curr->status='b';
			curr->nob++;
			curr->blockers=realloc(curr->blockers,curr->nob*sizeof(int));
			curr->blockers[(curr->nob)-1]=t->id;
			b_s++;
			blocked=(Node**)realloc(blocked,b_s*sizeof(Node*));
			Node *n=curr;
			blocked[b_s-1]= n;
			curr=Dequeue(ready);
			//printf("Starting %d\n",curr->id);
			swapcontext(&(n->thread),&(curr->thread));
			return 0;
		}
		return 0;
	}
	return -1;
}

// Join with all children
void MyThreadJoinAll(){
	int i,flag=0,c=0;
	for(i=0;i<curr->noc;i++){
		if(find(ready,curr->c_id[i])==1){
			flag=1;
			curr->nob++;
			curr->blockers=realloc(curr->blockers,curr->nob*sizeof(int));
			curr->blockers[(curr->nob)-1]=curr->c_id[i];
			c+=curr->c_id[i];
		}
	}
	//printf("Joining %d to %d\n",curr->id,c);
	if(flag==1){
		curr->status='b';
		b_s++;
		blocked=(Node**)realloc(blocked,b_s*sizeof(Node*));
		Node *n=curr;
		blocked[b_s-1]= n;
		curr=Dequeue(ready);
		//printf("Starting %d\n",curr->id);
		swapcontext(&(n->thread),&(curr->thread));
	}
}

// ****** SEMAPHORE OPERATIONS ****** 
// Create a semaphore
MySemaphore MySemaphoreInit(int initialValue){
	//printf("Sem init\n");
	if(initialValue<0){
		printf("Invalid Semaphore initial value\n");
		exit(-1);
	}
	Sem *s=malloc(1*sizeof(Sem));
	s->status='a';
	s->val=initialValue;
	s->id=s_id;
	s_id++;
	s->waiting=createQueue();
	sems=(Sem**)realloc(sems,s_id*sizeof(Sem));
	sems[s_id-1]=s;
	return (void *)s;
}

// Signal a semaphore
void MySemaphoreSignal(MySemaphore sem){
	//printf("Sem Sig\n");
	Sem *s=(Sem *)sem;
	if(s->status=='a'){	
		s->val++;
		if(s->waiting->size>0){
			Node* n=Dequeue(s->waiting);
			Enqueue(ready,n);
			s->val--;
		}
	}
	else{
		printf("Semaphore doesn't exist!\n");
		exit(-1);
	}
}

// Wait on a semaphore
void MySemaphoreWait(MySemaphore sem){
	//printf("Sem Wait\n");
	Sem *s=(Sem *)sem;
	if(s->status=='a'){
		if(s->val>0)
			s->val--;
		else{
			Node *n=curr;
			Enqueue(s->waiting,n);
			curr=Dequeue(ready);
			swapcontext(&(n->thread),&(curr->thread));
		}
	}
	else{
		printf("Semaphore doesn't exist!\n");
		exit(-1);
	}	
}

// Destroy on a semaphore
int MySemaphoreDestroy(MySemaphore sem){
	//printf("Sem destroy\n");
	Sem *s=(Sem *)sem;
	if(s->waiting->size>0)
		return -1;
	else
		s->status='d';
}

// ****** CALLS ONLY FOR UNIX PROCESS ****** 
// Create and run the "main" thread
void MyThreadInit(void(*start_funct)(void *), void *args){
	//printf("Initing\n");
	ready=createQueue();
	id=1;
	b_s=0;
	s_id=1;
	MyThreadCreate(start_funct, args);
	curr=Dequeue(ready);
	swapcontext(&ori,&(curr->thread));
	//printf("Back here\n");
}

int MyThreadInitExtra(){
	ready=createQueue();
	id=1;
	b_s=0;
	s_id=1;
	Node* n=(Node*)malloc(1*sizeof(Node));
	n->thread.uc_stack.ss_sp = malloc(8192);
  	n->thread.uc_stack.ss_size = 8192;
//ucontext_t m;
    n->thread.uc_link = &exitt;
	n->id=0;
	n->status='r';
	n->noc=0;
	n->nob=0;
	curr=n;
	swapcontext(&(curr->thread),&(curr->thread));
	return 0;
}