[bits 32]
section .text
global switch_to
switch_to:
    ; eip
    push esi
    push edi
    push ebx
    push ebp

    mov eax, [esp + 20] ; get arg: cur
    mov [eax], esp ; save esp into cur->kstack

    ; to a new context
    mov eax, [esp + 24] ; get arg: next
    mov esp, [eax] ; load esp from next->kstack

    ; pop from new context (not same as the beginning push)
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret
