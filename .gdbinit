# myos/.gdbinit

# Connect to QEMU as soon as GDB starts
target remote :1234

# Set up our preferred Text User Interface (TUI)
layout split
layout regs

# Enable logging to a file
set logging file gdb_session.log
set logging on

# Set the initial breakpoint on the program loader function.
# This will only be hit AFTER you type 'run ...' and press Enter.
b exec_program

# Define a script that will run automatically when breakpoint 1 (exec_program) is hit
commands 1
    # Info message for us
    printf "--- exec_program hit! Arming final breakpoint at iret... ---\n"
    
    # Set the real breakpoint to catch the context switch
    b kernel/cpu/isr.asm:151
    
    # Disable the temporary breakpoint so we don't hit it again
    disable 1
    
    # Continue execution automatically
    c
end

# Add a final message to the user
printf "--- .gdbinit setup complete. Type 'c' to boot the OS. ---\n"