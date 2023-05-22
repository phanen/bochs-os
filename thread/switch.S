[bits 32]
section .text
global switch_to
switch_to:
    ; eip
    push esi
    push edi
    push ebx
    push ebp

    ; context switch
    mov eax, [esp + 20] ; cur
    mov [eax], esp ; save esp into cur->kstack
    mov eax, [esp + 24] ; next
    mov esp, [eax] ; load esp from next->kstack

    ; pop from new context
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret ; goto eip of next thread
