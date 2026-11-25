// Host-only Windows SDK layout probe for the external oracle debugger.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstddef>
#include <cstdio>
int main() {
  static_assert(sizeof(void*) == 8);
  std::printf("{\"context_size\":%zu,\"context_flags\":%zu,\"eflags\":%zu,\"dr0\":%zu,\"dr1\":%zu,\"dr6\":%zu,\"dr7\":%zu,\"rcx\":%zu,\"rdx\":%zu,\"r8\":%zu,\"rsp\":%zu,\"rip\":%zu,",
    sizeof(CONTEXT),offsetof(CONTEXT,ContextFlags),offsetof(CONTEXT,EFlags),
    offsetof(CONTEXT,Dr0),offsetof(CONTEXT,Dr1),offsetof(CONTEXT,Dr6),offsetof(CONTEXT,Dr7),
    offsetof(CONTEXT,Rcx),offsetof(CONTEXT,Rdx),offsetof(CONTEXT,R8),offsetof(CONTEXT,Rsp),offsetof(CONTEXT,Rip));
  std::printf("\"event_size\":%zu,\"event_union\":%zu,\"exception_address\":%zu,\"exception_first_chance\":%zu,\"startup_size\":%zu,\"startup_flags\":%zu,\"stdin\":%zu,\"stdout\":%zu,\"stderr\":%zu,",
    sizeof(DEBUG_EVENT),offsetof(DEBUG_EVENT,u),offsetof(EXCEPTION_RECORD,ExceptionAddress),
    offsetof(EXCEPTION_DEBUG_INFO,dwFirstChance),sizeof(STARTUPINFOW),offsetof(STARTUPINFOW,dwFlags),
    offsetof(STARTUPINFOW,hStdInput),offsetof(STARTUPINFOW,hStdOutput),offsetof(STARTUPINFOW,hStdError));
  std::printf("\"pi_size\":%zu,\"pi_pid\":%zu,\"pi_tid\":%zu,\"create_image_base\":%zu,\"create_process_handle\":%zu,\"create_thread_handle\":%zu,",
    sizeof(PROCESS_INFORMATION),offsetof(PROCESS_INFORMATION,dwProcessId),offsetof(PROCESS_INFORMATION,dwThreadId),
    offsetof(CREATE_PROCESS_DEBUG_INFO,lpBaseOfImage),offsetof(CREATE_PROCESS_DEBUG_INFO,hProcess),offsetof(CREATE_PROCESS_DEBUG_INFO,hThread));
  const std::size_t regs[]={offsetof(CONTEXT,Rax),offsetof(CONTEXT,Rcx),offsetof(CONTEXT,Rdx),offsetof(CONTEXT,Rbx),offsetof(CONTEXT,Rsp),offsetof(CONTEXT,Rbp),offsetof(CONTEXT,Rsi),offsetof(CONTEXT,Rdi),offsetof(CONTEXT,R8),offsetof(CONTEXT,R9),offsetof(CONTEXT,R10),offsetof(CONTEXT,R11),offsetof(CONTEXT,R12),offsetof(CONTEXT,R13),offsetof(CONTEXT,R14),offsetof(CONTEXT,R15)};
  std::printf("\"gpr_offsets\":[");for(unsigned i=0;i<16;++i)std::printf("%s%zu",i?",":"",regs[i]);std::puts("]}");
}
