**ezCoroutine Lib in 300 lines of C code (with some gcc inline assembly)**

Overall Design:
We use circular linked list to link all the task control blocks, which contain all the info needed to restore program's context such as IP, BP, SP registers, thread identity and so on. The trick is in inline assembly. We restore some stack-related registers, then to use X86 jump instruction to force IP to point to the IP register of next task control blocks saved earlier, so the next task comes into action. Simple enough to read the source code directly.

Test Environment:
X86 / Ubuntu 17.04, MacOSX 10.6.0, Windows7/ GNU GCC version 6.3.0, LLVM 3.5svn, MinGW / Posix Standard

Note: demo3.c and echoserver.c are compatible with only unix-like systems. demo1.c and demo2.c written in pure C could be compiled and run on both X86 unix-like systems and windows systems.

More Detail: [cnblogs in chinese](http://www.cnblogs.com/github-Yuandong-Chen/p/6849168.html "Implementation Tutorial")
