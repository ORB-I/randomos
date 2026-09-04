[bits 64]
global _start
extern _libc_setup

section .text
_start:
    call _libc_setup ; we dont call main anymore because _libc_setup wll
                     ; copy environ off the stack and call main for us
                     ; also we dont needa do setup since ld.so just invokes
                     ; this like a C function
    ret