#include <dos.h>
#include <stdlib.h>
#include <stdio.h>


int current;
long timecount;
#define NTCB 3

#define FINISHED 0
#define RUNNING 1
#define READY   2
#define BLOCKED 3

#define TIMEINT 0x08

#define TIMESLIP 5

#define STACKLEN 1024

struct TCB{
	unsigned char *stack; /*线程堆栈的起始地址*/
	unsigned  int ss;     /*堆栈段址*/
	unsigned  int sp;	/*堆栈指针*/
	int state;
	char name[10];
	unsigned  int priority;

}tcb[NTCB];

void interrupt (*old_int8)(void);
void interrupt new_int8(void);
void interrupt test_int8(void);
void over(void);

#define GET_INDOS 0x34
#define GET_CRIT_ERR 0x5d06

char far *indos_ptr=0;
char far *crit_err_ptr=0;

int DosBusy(void);
void InitInDos(void);

void InitInDos(void)
{
 union REGS regs;
 struct SREGS segregs;

 regs.h.ah=GET_INDOS;
 intdosx(&regs, &regs, &segregs);
 indos_ptr=MK_FP(segregs.es, regs.x.bx);


 if(_osmajor<3)
   crit_err_ptr=indos_ptr+1;
 else if(_osmajor==3&&_osminor==0)
   crit_err_ptr=indos_ptr-1;
 else
 {
   regs.x.ax=GET_CRIT_ERR;
   intdosx(&regs, &regs, &segregs);
   crit_err_ptr=MK_FP(segregs.ds, regs.x.si);
 }
}

int DosBusy(void)
{
 if(indos_ptr&&crit_err_ptr)
   return(*indos_ptr||*crit_err_ptr);
 else
   return -1;
}

typedef void (far *funcptr)(void);
int create(char *name, funcptr func, int stlen);



void p1( )
{
	long i, j, k;

	for(i=0; i<40; i++)
	{
		putchar('a');

		for(j=0; j<1000; j++)
			for(k=0; k<20000; k++);
	}
}

void p2( )
{
	long i, j, k;

	for(i=0; i<20; i++)
	{
		putchar('b');

		for(j=0; j<1000; j++)
			for(k=0;k<20000; k++);
	}
}

int Find()
{
	int i,j;

	i=current;

	while( tcb[i=((i+1)%NTCB)].state!=READY || i==current );


	return i;
}

void interrupt swtch()            /* 其他原因CPU调度  */
{
	int i;

	if(tcb[current].state!=FINISHED
		&&current!=0) /* 当前线程还没结束 */
		return;

	i=Find();
	if(i<0)
		return;

	disable();
	tcb[current].ss=_SS;
	tcb[current].sp=_SP;

	if(tcb[current].state==RUNNING)
		tcb[current].state=READY;      /* 放入就绪队列中 */

	_SS=tcb[i].ss;
	_SP=tcb[i].sp;        /* 保存现场 */

	tcb[i].state=RUNNING;
	current=i;
	enable();
}

void over()
{
	if(tcb[current].state==RUNNING)
	{
		disable();
		tcb[current].state=FINISHED;
		strcpy(tcb[current].name,NULL);
		free(tcb[current].stack);
		enable();
	}

	swtch();
}

void InitTcb()
{
	unsigned int *tmp=0;

	//for thread 1

	tcb[1].state=READY;
//	strcpy(tcb[1].name, "p1");

	tcb[1].stack=(unsigned char *)malloc(STACKLEN);
	memset(tcb[1].stack, 0xff, STACKLEN);

	tmp=(unsigned int *)(tcb[1].stack+STACKLEN-2);
	
	*tmp=FP_SEG(over);
	*(tmp-1)=FP_OFF(over);
	*(tmp-2)=0x200;	
	*(tmp-3)=FP_SEG(p1);
	*(tmp-4)=FP_OFF(p1);
	
	*(tmp-9)=_ES;
	*(tmp-10)=_DS;
	tcb[1].ss=FP_SEG(tmp-13);
	tcb[1].sp=FP_OFF(tmp-13);


	//for thread 2

	tcb[2].state=READY;
//	strcpy(tcb[2].name, "p2");

	tcb[2].stack=(unsigned char *)malloc(STACKLEN);
	memset(tcb[2].stack, 0xff, STACKLEN);
	
	tmp=(unsigned int *)(tcb[2].stack+STACKLEN-2);

	*tmp=FP_SEG(over);
	*(tmp-1)=FP_OFF(over);
	*(tmp-2)=0x0200;	
	*(tmp-3)=FP_SEG(p2);
	*(tmp-4)=FP_OFF(p2);
	
	*(tmp-9)=_ES;
	*(tmp-10)=_DS;
	tcb[2].ss=FP_SEG(tmp-13);
	tcb[2].sp=FP_OFF(tmp-13);
}

void interrupt new_int8(void)
{
	int i;

	(*old_int8)();
	timecount++;

	if(timecount!=TIMESLIP)
		return;
	else
	{
		if(DosBusy())
			return;
		else
		{
			disable();
//			asm CLI

			tcb[current].ss=_SS;
			tcb[current].sp=_SP;

			if(tcb[current].state==RUNNING)
				tcb[current].state=READY;

			i=Find();

			if(i==current)
				return;

			
			_SS=tcb[i].ss;
			_SP=tcb[i].sp;
			tcb[i].state=RUNNING;

			timecount=0;
			current=i;

			enable();
//			asm STI

		}
	}
}

void interrupt test_int8()
{
	timecount++;
	printf("timer!\n");
}

void tcb_state()
{
	int i;

	for(i=0; i<NTCB; i++)
	{
		switch( tcb[i].state )
		{
		case FINISHED:
				printf("\nthe thread %d is finished!\n", i);
				break;
		case RUNNING:
				printf("the thread %d is running!\n", i);
				break;
		case READY:
				printf("the thread %d is ready!\n", i);
				break;
		case BLOCKED:
				printf("the thread %d is blocked!\n", i);
				break;
		default:
				printf("unknown state of thread %!\n", i);
		}
	}
}

int all_finished()
{
	int i;

	if(tcb[1].state!=FINISHED||tcb[2].state!=FINISHED)
		return -1;
	else 
		return 0;
}

void releaseTcb()
{
	int i=0;

	for(i=1; i<NTCB; i++)
	{
		if(tcb[i].stack)
			free(tcb[i].stack);
	}
}

void main()
{
	timecount=0;

	InitInDos();
	InitTcb();

	old_int8=getvect(TIMEINT);

	strcpy(tcb[0].name, "main");
	tcb[0].state=RUNNING;
	current=0;

	tcb_state();

	disable();
	setvect(TIMEINT, new_int8);
	enable();

	while(all_finished())
	{
	//	printf("system running!\n");	
	}


	tcb[0].name[0]='\0';
	tcb[0].state=FINISHED;
	setvect(TIMEINT, old_int8);

	tcb_state();

	printf("\n Multi_task system terminated.\n");
}



