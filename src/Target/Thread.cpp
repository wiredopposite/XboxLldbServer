#include "Target/Thread.h"
#include "Target/Process.h"
#include "Host/Platform.h"

#include <cstring>

#define super ds2::Target::ThreadBase

namespace ds2 {
namespace Target {

namespace {

// Xbox CONTEXT is flat-segmented: only SegCs and SegSs exist, the other data
// segments are implicitly the same flat selector. SSE state lives inline in
// FLOATING_SAVE_AREA (XmmRegisterArea + MXCsr) on Xbox.
void contextToState(CONTEXT const &ctx, Architecture::CPUState &state) {
  state.clear();
  state.gp.eax = ctx.Eax;
  state.gp.ecx = ctx.Ecx;
  state.gp.edx = ctx.Edx;
  state.gp.ebx = ctx.Ebx;
  state.gp.esi = ctx.Esi;
  state.gp.edi = ctx.Edi;
  state.gp.esp = ctx.Esp;
  state.gp.ebp = ctx.Ebp;
  state.gp.eip = ctx.Eip;
  state.gp.eflags = ctx.EFlags;
  state.gp.cs = ctx.SegCs;
  state.gp.ss = ctx.SegSs;
  // Xbox is flat: GDB still expects ds/es/fs/gs, mirror ss for them.
  state.gp.ds = ctx.SegSs;
  state.gp.es = ctx.SegSs;
  state.gp.fs = ctx.SegSs;
  state.gp.gs = ctx.SegSs;

  state.x87.fctw = static_cast<uint16_t>(ctx.FloatSave.ControlWord);
  state.x87.fstw = static_cast<uint16_t>(ctx.FloatSave.StatusWord);
  state.x87.ftag = static_cast<uint16_t>(ctx.FloatSave.TagWord);
  state.x87.fop = static_cast<uint16_t>(ctx.FloatSave.ErrorOpcode);
  state.x87.fiseg = ctx.FloatSave.ErrorSelector;
  state.x87.fioff = ctx.FloatSave.ErrorOffset;
  state.x87.foseg = ctx.FloatSave.DataSelector;
  state.x87.fooff = ctx.FloatSave.DataOffset;
  for (size_t i = 0; i < 8; ++i) {
    std::memcpy(state.x87.regs[i].data,
                ctx.FloatSave.RegisterArea + i * 10,
                sizeof(state.x87.regs[i].data));
  }

  state.sse.mxcsr = ctx.FloatSave.MXCsr;
  for (size_t i = 0; i < 8; ++i) {
    std::memcpy(&state.sse.regs[i],
                ctx.FloatSave.XmmRegisterArea + i * 16,
                16);
  }
}

void stateToContext(Architecture::CPUState const &state, CONTEXT &ctx) {
  ctx.Eax = state.gp.eax;
  ctx.Ecx = state.gp.ecx;
  ctx.Edx = state.gp.edx;
  ctx.Ebx = state.gp.ebx;
  ctx.Esi = state.gp.esi;
  ctx.Edi = state.gp.edi;
  ctx.Esp = state.gp.esp;
  ctx.Ebp = state.gp.ebp;
  ctx.Eip = state.gp.eip;
  ctx.EFlags = state.gp.eflags;
  ctx.SegCs = state.gp.cs;
  ctx.SegSs = state.gp.ss;

  ctx.FloatSave.ControlWord = state.x87.fctw;
  ctx.FloatSave.StatusWord = state.x87.fstw;
  ctx.FloatSave.TagWord = state.x87.ftag;
  ctx.FloatSave.ErrorOpcode = state.x87.fop;
  ctx.FloatSave.ErrorSelector = state.x87.fiseg;
  ctx.FloatSave.ErrorOffset = state.x87.fioff;
  ctx.FloatSave.DataSelector = state.x87.foseg;
  ctx.FloatSave.DataOffset = state.x87.fooff;
  for (size_t i = 0; i < 8; ++i) {
    std::memcpy(ctx.FloatSave.RegisterArea + i * 10,
                state.x87.regs[i].data,
                sizeof(state.x87.regs[i].data));
  }

  ctx.FloatSave.MXCsr = state.sse.mxcsr;
  for (size_t i = 0; i < 8; ++i) {
    std::memcpy(ctx.FloatSave.XmmRegisterArea + i * 16,
                &state.sse.regs[i],
                16);
  }
}

} // namespace

Thread::Thread(Process *process, ThreadId tid, PETHREAD ethread)
    : super(process, tid), _ethread(ethread) {}

Thread::~Thread() = default;

ErrorCode Thread::terminate() {
  // Killing arbitrary kernel threads is not supported via the debug session.
  return kErrorUnsupported;
}

ErrorCode Thread::suspend() {
  if (_ethread == nullptr) {
    return kErrorInvalidArgument;
  }
  KeSuspendThread(reinterpret_cast<PKTHREAD>(_ethread));
  _state = kStopped;
  _stopInfo.event = StopInfo::kEventStop;
  _stopInfo.reason = StopInfo::kReasonNone;
  return kSuccess;
}

ErrorCode Thread::resume(int /*signal*/, Address const &address) {
  // The currently-stopped thread is parked inside Process::onTrap waiting on
  // _resumeRequested; it resumes when Process::resume() (called via the
  // resumeAll path in DebugSessionImpl) signals that event. For *other*
  // threads we suspended for all-stop, KeResumeThread is the answer.
  Process *proc = static_cast<Process *>(_process);
  Thread *current = static_cast<Thread *>(proc->currentThread());

  if (this == current) {
    if (address.valid()) {
      CHK(modifyRegisters(
          [&address](Architecture::CPUState &s) { s.setPC(address); }));
    }
    // Releases _resumeRequested in the parked trap handler. The handler
    // wakes, applies any staged modifications, and returns TRUE to the
    // kernel — which resumes the user thread.
    proc->releaseStoppedThread();
    _state = kRunning;
    return kSuccess;
  }

  if (_ethread != nullptr) {
    KeResumeThread(reinterpret_cast<PKTHREAD>(_ethread));
  }
  _state = kRunning;
  return kSuccess;
}

ErrorCode Thread::step(int signal, Address const &address) {
  // Set TF (single step) on the captured context, then resume normally.
  CHK(modifyRegisters(
      [](Architecture::CPUState &s) { s.gp.eflags |= (1u << 8); }));
  CHK(resume(signal, address));
  _state = kStepped;
  return kSuccess;
}

ErrorCode Thread::readCPUState(Architecture::CPUState &state) {
  Process *proc = static_cast<Process *>(_process);
  if (this != proc->currentThread()) {
    // Reading other threads' registers requires walking their saved kernel
    // trap frame; not implemented yet.
    return kErrorUnsupported;
  }
  CONTEXT ctx;
  CHK(proc->getCapturedContext(ctx));
  contextToState(ctx, state);
  return kSuccess;
}

ErrorCode Thread::writeCPUState(Architecture::CPUState const &state) {
  Process *proc = static_cast<Process *>(_process);
  if (this != proc->currentThread()) {
    return kErrorUnsupported;
  }
  CONTEXT ctx;
  CHK(proc->getCapturedContext(ctx));
  stateToContext(state, ctx);
  return proc->setModifiedContext(ctx);
}

void Thread::updateState() {}

} // namespace Target
} // namespace ds2
