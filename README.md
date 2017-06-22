It's an asymmetric coroutine library (like lua).

You can use coroutine_open to open a schedule first, and then create coroutine in that schedule. 

You should call coroutine_resume in the thread that you call coroutine_open, and you **CAN** call it in a coroutine in the same schedule.(Which makes our lib **Far** stronger than cloudwu's tiny coroutine lib. Easy... my friend, we use gasm inline assembly)

Coroutines in the same schedule share the stack , so you can create many coroutines without worry about memory.
But switching context will copy the stack the coroutine used. Most importantly, you should be careful about local variables in coroutines which are designed to receive some kernal data sent by OS, if it's the case, you are required to malloc a space in heap to receive these kind of data. (or kernal might write all data to the same place, and BOOM! All the data are overlapped because we use shared stack in coroutines)

Read source for detail.(300 lines of C code is not difficult)

Test Environment:
X86 / Ubuntu 17.04, MacOSX 10.6.0, Windows7/ GNU GCC version 6.3.0, LLVM 3.5svn, MinGW

More Detail: [cnblogs in chinese](http://www.cnblogs.com/github-Yuandong-Chen/p/6973932.html "Implementation Tutorial")
