; boot.asm
bits 32

section .multiboot
align 8
multiboot_start:
    dd 0xe85250d6                ; Magic number for Multiboot2
    dd 0                         ; Architecture 0 (protected mode i386)
    dd multiboot_end - multiboot_start ; Header length
    dd -(0xe85250d6 + 0 + (multiboot_end - multiboot_start)) ; Checksum

    ; TAG: Request Framebuffer Video Mode
    align 8
    dw 5                         ; Type 5: Framebuffer tag
    dw 0                         ; Flags
    dd 20                        ; Size of this tag
    dd 1920                      ; Requested Width
    dd 1080                      ; Requested Height
    dd 32                        ; Requested Bits Per Pixel (Depth)

    ; TAG: End tag
    align 8
    dw 0
    dw 0
    dd 8
multiboot_end:

section .text
global _start
_start:
    cli                          ; Disable hardware interrupts
    mov esp, stack_top           ; Set up our stack pointer
    
    push ebx                     ; Multiboot2 stores the info pointer in EBX
    push eax                     ; and the magic number in EAX
    
    extern kernel_main
    call kernel_main             ; Jump into our C kernel code

.halt:
    hlt                          ; If kernel returns, halt the CPU
    jmp .halt

section .bss
align 16
stack_bottom:
    resb 16384                   ; 16 KB of stack memory
stack_top:
