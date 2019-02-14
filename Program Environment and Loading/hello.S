  .globl _start
  .data
  hello:
        .ascii  "Hello, world!\n"
        hellolen = . - hello

  .text
  _start:
	movl	$hellolen,%edx
	movl	$hello,%ecx   
	movl	$1,%ebx
	movl	$4,%eax 
        int 	$0x80        

	movl    $0,%ebx
        movl    $1,%eax      
        int     $0x80        


