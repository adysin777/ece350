.syntax unified //using unified syntax for ARM and Thumb instructions
.cpu cortex-m4 //tells Assembler to generate instructions for Cortex-M4 
.thumb //tells Assembler to use Thumb instruction set


.global SVC_Handler //indicates to the linker that this function exists
.thumb_func //marks the following symbol as a Thumb function *Required above every function, otherwise will Hardfault! 
SVC_Handler: //function name
//This function serves as a wrapper of SVC_Handler_Main, and passes in the correct stack pointer
	TST LR, 4 //Tests bit2 in LR (4 is 0b100, i.e. bit2 is 1)
	ITE EQ //If Then Else, Equal condition
	MRSEQ R0, MSP //If bit2 was 0, processor was using MSP as stack pointer --copy MSP to R0 in order to pass it into the C function
	MRSNE R0, PSP //Otherwise, processor was using PSP, pass in PSP
	B SVC_Handler_Main //Go to the C function, where we can write kernel code conveniently in C


.global PendSV_Handler
.thumb_func
PendSV_Handler:
//Context switch: save outgoing task's registers, pick the next task in C,
//restore its registers, and return into it using PSP.
//Saved software block (low to high address): R4-R11, EXC_RETURN [, S16-S31 if FP frame].
	MRS R0, PSP //R0 = outgoing task's stack (points at the stacked hardware frame)
	CBZ R0, PendSV_Load //PSP==0 means first switch (set by k_kernel_start) -- nothing to save
	TST LR, 0x10 //EXC_RETURN bit4==0 means the task has an FP (extended) frame
	IT EQ
	VSTMDBEQ R0!, {S16-S31} //save callee-saved FP regs only for FP frames (lazy stacking handles S0-S15)
	STMDB R0!, {R4-R11, LR} //save callee-saved core regs + this task's EXC_RETURN; R0 == saved_psp
PendSV_Load:
	BL PendSV_C_Handler //C: saves outgoing PSP, runs scheduler; arg in R0, returns next task's saved_psp in R0
	LDMIA R0!, {R4-R11, LR} //restore core regs + the incoming task's EXC_RETURN
	TST LR, 0x10 //did the incoming task stack an FP frame?
	IT EQ
	VLDMIAEQ R0!, {S16-S31} //if so, restore its callee-saved FP regs
	MSR PSP, R0 //PSP = incoming task's hardware frame
	BX LR //return from exception -- CPU pops the hardware frame and runs the task