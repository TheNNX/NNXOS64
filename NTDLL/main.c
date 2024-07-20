#include <scheduler.h>
#include <object.h>
#include <bugcheck.h>
#include <mm.h>

#define SYSCALL4PLUS(x) \
__pragma(comment(linker, "/export:"#x##"=Syscall"#x))\
void Syscall##x();

#include <syscall.inc>