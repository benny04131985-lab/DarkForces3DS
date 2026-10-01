@ DF3DS_RET1_AS_ARM_TRAMPOLINE_V61
.global armFunc
.global armFuncR0
.global armFuncR0R1
.global armFuncObjLast
.global armFuncR0ObjLast
armFunc:
    stmdb sp!, {r4-r8, lr}
    mov r6, r0
    movs r7, r1
    mov r4, r2
    mov r8, #0
    beq nomoreargs
    cmp r7, #4
    ldrge r0, [r6],#4
    cmp r7, #2*4
    ldrge r1, [r6],#4
    cmp r7, #3*4
    ldrge r2, [r6],#4
    cmp r7, #4*4
    ldrge r3, [r6],#4
    ble nomoreargs
    sub r7, r7, #4*4
    add r8, r7, #4
    bic r8, r8, #4
    sub sp, sp, r8
    mov r12, sp
stackargsloop:
    ldr r5, [r6], #4
    str r5, [sp], #4
    subs r7, r7, #4
    bne stackargsloop
    mov sp, r12
nomoreargs:
    blx r4
    add sp, sp, r8
    ldmia sp!, {r4-r8, pc}
armFuncObjLast:
    stmdb sp!, {r4-r8, lr}
    mov r6, r0
    movs r7, r1
    mov r4, r2
    mov r8, #0
    mov r0, r3
    mov r5, r3
    beq nomoreargsarmFuncObjLast
    cmp r7, #4
    ldrge r0, [r6],#4
    cmp r7, #2*4
    ldrge r1, [r6],#4
    movlt r1, r5
    cmp r7, #3*4
    ldrge r2, [r6],#4
    movlt r2, r5
    cmp r7, #4*4
    ldrge r3, [r6],#4
    movlt r3, r5
    blt nomoreargsarmFuncObjLast
    sub r7, r7, #4*4
    add r8, r7, #8
    bic r8, r8, #4
    str r5, [sp,#-4]
    str r5, [sp,#-8]
    sub sp, sp, r8
    cmp r7, #0
    beq nomoreargsarmFuncObjLast
    mov r12, sp
stackargslooparmFuncObjLast:
    ldr r5, [r6], #4
    str r5, [sp], #4
    subs r7, r7, #4
    bne stackargslooparmFuncObjLast
    mov sp, r12
nomoreargsarmFuncObjLast:
    blx r4
    add sp, sp, r8
    ldmia sp!, {r4-r8, pc}
armFuncR0ObjLast:
    stmdb sp!, {r4-r8, lr}
    ldr r5, [sp,#6*4]
    mov r6, r0
    movs r7, r1
    mov r4, r2
    mov r8, #0
    mov r0, r3
    mov r1, r5
    beq nomoreargsarmFuncR0ObjLast
    cmp r7, #1*4
    ldrge r1, [r6],#4
    cmp r7, #2*4
    ldrge r2, [r6],#4
    movlt r2, r5
    cmp r7, #3*4
    ldrge r3, [r6],#4
    movlt r3, r5
    blt nomoreargsarmFuncR0ObjLast
    sub r7, r7, #3*4
    add r8, r7, #8
    bic r8, r8, #4
    str r5, [sp,#-4]
    str r5, [sp,#-8]
    sub sp, sp, r8
    cmp r7, #0
    beq nomoreargsarmFuncR0ObjLast
    mov r12, sp
stackargslooparmFuncR0ObjLast:
    ldr r5, [r6], #4
    str r5, [sp], #4
    subs r7, r7, #4
    bne stackargslooparmFuncR0ObjLast
    mov sp, r12
nomoreargsarmFuncR0ObjLast:
    blx r4
    add sp, sp, r8
    ldmia sp!, {r4-r8, pc}
armFuncR0:
    stmdb sp!, {r4-r8, lr}
    mov r6, r0
    movs r7, r1
    mov r4, r2
    mov r8, #0
    mov r0, r3
    beq nomoreargsarmFuncR0
    cmp r7, #1*4
    ldrge r1, [r6],#4
    cmp r7, #2*4
    ldrge r2, [r6],#4
    cmp r7, #3*4
    ldrge r3, [r6],#4
    ble nomoreargsarmFuncR0
    sub r7, r7, #3*4
    add r8, r7, #4
    bic r8, r8, #4
    sub sp, sp, r8
    mov r12, sp
stackargslooparmFuncR0:
    ldr r5, [r6], #4
    str r5, [sp], #4
    subs r7, r7, #4
    bne stackargslooparmFuncR0
    mov sp, r12
nomoreargsarmFuncR0:
    blx r4
    add sp, sp, r8
    ldmia sp!, {r4-r8, pc}
armFuncR0R1:
    stmdb sp!, {r4-r8, lr}
    mov r6, r0
    movs r7, r1
    mov r4, r2
    mov r8, #0
    mov r0, r3
    ldr r1, [sp, #6*4]
    beq nomoreargsarmFuncR0R1
    cmp r7, #1*4
    ldrge r2, [r6],#4
    cmp r7, #2*4
    ldrge r3, [r6],#4
    ble nomoreargsarmFuncR0R1
    sub r7, r7, #2*4
    add r8, r7, #4
    bic r8, r8, #4
    sub sp, sp, r8
    mov r12, sp
stackargslooparmFuncR0R1:
    ldr r5, [r6], #4
    str r5, [sp], #4
    subs r7, r7, #4
    bne stackargslooparmFuncR0R1
    mov sp, r12
nomoreargsarmFuncR0R1:
    blx r4
    add sp, sp, r8
    ldmia sp!, {r4-r8, pc}
