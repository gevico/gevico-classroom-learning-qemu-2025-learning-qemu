    .option    norvc

    .text
    .global _start
    .global crt_abort

_start:
    lla     t0, trap
    csrw    mtvec, t0

    # sp size 0x400000
    csrr    a0, mhartid
    slli    a1, a0, 22
    # sp base 0x84000000
    li      sp, 0x84000000
    # TODO: initiation bsp
    call    _init_bsp
    # jump main function
    call    main
    # Success!
    li      a0, 0
    j       _exit

_init_bsp:
    addi    sp, sp, -32
    sd      t0, 16(sp)
    sd      t1, 8(sp)
    sd      t2, 0(sp)
    
    # Set up machine trap vector
    li      t2, 0x800000000024112d
    csrw    0x301, t2
    li      t0, 0x2200
    csrr    t1, mstatus
    or      t1, t1, t0
    csrw    mstatus, t1

    # Enable global interrupts in mstatus
    csrsi   mstatus, 0x8
    
    # Enable external interrupts in mie register
    # MEIE (Machine External Interrupt Enable) bit = 11
    li      t0, 0x800
    csrw    mie, t0
    
    # Set up PLIC for external interrupt source 3 (SPI0)
    # PLIC base address: 0xc000000
    li      t0, 0xc000000

    # Set priority for interrupt source 3 (SPI0)
    li      t1, 0x0         # Priority base
    add     t1, t0, t1
    li      t2, 7            # Set priority to 7 for source 3
    sw      t2, 12(t1)      # Write priority for source 3
    
    # Set interrupt threshold for hart 0
    # Context base: 0x200000, hart 0 offset: 0
    li      t1, 0x200000    # Context base
    add     t1, t0, t1
    li      t2, 0x0         # Threshold 0 (accept all interrupts)
    sw      t2, 0(t1)       # Write threshold

    # Set interrupt claim for hart 0
    # Claim base: 0x200004, hart 0 offset: 4
    li      t1, 0x200004    # Claim base
    add     t1, t0, t1
    li      t2, 0x0         # Initialize claim register to 0
    sw      t2, 0(t1)       # Write to claim register

    # Enable interrupt source 3 for hart 0
    li      t1, 0x2000      # Enable base
    add     t1, t0, t1
    li      t2, 0x8         # Enable bit 3 (interrupt source 3)
    sw      t2, 0(t1)       # Write enable register

    ld      t2, 0(sp)
    ld      t1, 8(sp)
    ld      t0, 16(sp)
    addi    sp, sp, 32
    ret

trap:
    # Save registers
    addi    sp, sp, -32
    sd      t0, 24(sp)
    sd      t1, 16(sp)
    sd      t2, 8(sp)
    sd      t3, 0(sp)
    
    # Check if this is an interrupt or exception
    csrr    t0, mcause
    bltz    t0, handle_interrupt
    
    # Handle exception (instruction trap)
    csrr    t1, mepc
    csrr    t2, mtval
    lwu     t3, 0(t1)
    bne     t2, t3, fail

    # Skip the insn and continue.
    addi    t1, t1, 4
    csrw    mepc, t1
    j       trap_exit

handle_interrupt:
    # Check interrupt type
    andi    t1, t0, 0x7FF  # Extract interrupt number
    li      t2, 11         # Machine external interrupt
    beq     t1, t2, handle_external_interrupt
    
    # Unknown interrupt, continue
    j       trap_exit

handle_external_interrupt:
    # Read PLIC interrupt source
    # PLIC claim register is at PLIC_BASE + CONTEXT_BASE + 4 (for hart 0)
    li      t1, 0xc200004    # PLIC claim register address
    lw      t2, 0(t1)        # Read interrupt source
    
    # Check if it's SPI0 interrupt (source 3)
    li      t3, 3

    # Save context before calling functions
    addi    sp, sp, -32      # Allocate stack space for 8 registers
    sw      ra, 0(sp)        # Save return address
    sw      t0, 4(sp)        # Save t0
    sw      t1, 8(sp)        # Save t1
    sw      t2, 12(sp)       # Save t2
    sw      t3, 16(sp)       # Save t3
    sw      t4, 20(sp)       # Save t4
    sw      t5, 24(sp)       # Save t5
    sw      t6, 28(sp)       # Save t6

    # Save context before calling functions
    bne     t2, t3, handle_other_external_interrupt

    # It's SPI0 interrupt, call handler
    call    spi0_interrupt_handler

    # Restore context
    lw      ra, 0(sp)        # Restore return address
    lw      t0, 4(sp)        # Restore t0
    lw      t1, 8(sp)        # Restore t1
    lw      t2, 12(sp)       # Restore t2
    lw      t3, 16(sp)       # Restore t3
    lw      t4, 20(sp)       # Restore t4
    lw      t5, 24(sp)       # Restore t5
    lw      t6, 28(sp)       # Restore t6
    addi    sp, sp, 32       # Deallocate stack space

    # Acknowledge the interrupt by writing back to claim register
    sw      t2, 0(t1)
    j       trap_exit

handle_other_external_interrupt:
    # Restore context
    lw      ra, 0(sp)        # Restore return address
    lw      t0, 4(sp)        # Restore t0
    lw      t1, 8(sp)        # Restore t1
    lw      t2, 12(sp)       # Restore t2
    lw      t3, 16(sp)       # Restore t3
    lw      t4, 20(sp)       # Restore t4
    lw      t5, 24(sp)       # Restore t5
    lw      t6, 28(sp)       # Restore t6
    addi    sp, sp, 32       # Deallocate stack space

    # Acknowledge the interrupt by writing back to claim register
    sw      t2, 0(t1)

    j       trap_exit

trap_exit:
    # Restore registers
    ld      t3, 0(sp)
    ld      t2, 8(sp)
    ld      t1, 16(sp)
    ld      t0, 24(sp)
    addi    sp, sp, 32
    mret

crt_abort:
fail:
    li    a0, 1

# Exit code in a0
_exit:
    lla    a1, semiargs
    li    t0, 0x20026    # ADP_Stopped_ApplicationExit
    sd    t0, 0(a1)
    sd    a0, 8(a1)
    li    a0, 0x20    # TARGET_SYS_EXIT_EXTENDED

    # Semihosting call sequence
    .balign    16
    slli    zero, zero, 0x1f
    ebreak
    srai    zero, zero, 0x7
    j    .

    .text
    .weak      spi0_interrupt_handler
spi0_interrupt_handler:
    # Default empty interrupt handler
    # This will be overridden if spi_interrupt_handler is defined in the test
    ret

    .data
    .balign    16
semiargs:
    .space     16
