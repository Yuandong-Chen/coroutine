NOTE: MacOSX is now 10.14.X, any 32 bit program cannot be compiled and 64 bit C library is altered, so current coroutine is not supported under 10.14.X.

It's an asymmetric coroutine library (like lua).

You can use coroutine_open to open a schedule first, and then create coroutine in that schedule. 

You should call coroutine_resume in the thread that you call coroutine_open, and you are **Allowed** to call it in a coroutine in the same schedule.(We use arch-related jmp_buf structure and setjmp/longjmp to implement this X64 version. We also assume a compiler-related call stack layout. Right now, it's for MacOSX only.)

Coroutines in the same schedule share the stack , so you can create many coroutines without worry about memory.
But switching context will copy the stack the coroutine used. Most importantly, you should be careful about local variables in coroutines which are designed to receive some kernal data sent by OS, if it's the case, you are required to malloc a space in heap to receive these kind of data. (or kernal might write all data to the same place, and BOOM! All the data are overlapped because we use shared stack in coroutines)

Read source for detail.

The Only Supported Environment:
MacBook Air: X64 / MacOSX 10.9.5/ LLVM 3.5svn

More Detail: [cnblogs in chinese](http://www.cnblogs.com/github-Yuandong-Chen/p/6973932.html "Implementation Tutorial")
