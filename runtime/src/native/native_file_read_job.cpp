#include "mka/native_file_read_job.hpp"
#include "mka/content_volume.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <process.h>
#include <cerrno>
#endif
namespace mka::content {
struct NativeFileReadJob::State {
    const std::thread::id owner=std::this_thread::get_id();
    std::vector<std::uint8_t> data;std::uint64_t offset{};
    std::atomic<NativeFileReadStatus> status{NativeFileReadStatus::pending};
    std::atomic<bool> cancelling{false};NativeFileReadFailure failure;
    std::shared_ptr<const void> volume;
#ifdef _WIN32
    HANDLE file=INVALID_HANDLE_VALUE,event=nullptr,worker=nullptr;OVERLAPPED work{};
    ~State(){
        if(worker&&status.load()==NativeFileReadStatus::pending){std::fputs("native file read destroyed before terminal completion\n",stderr);std::terminate();}
        if(worker)CloseHandle(worker);if(event)CloseHandle(event);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
    }
    void run()noexcept{
        std::size_t done=0;
        while(done<data.size()){
            if(cancelling.load()){status.store(NativeFileReadStatus::cancelled);return;}
            work={};work.hEvent=event;const auto at=offset+done;work.Offset=DWORD(at);work.OffsetHigh=DWORD(at>>32);
            const DWORD issued=DWORD(std::min<std::size_t>(data.size()-done,MAXDWORD));ResetEvent(event);
            const BOOL ok=ReadFile(file,data.data()+done,issued,nullptr,&work);
            const auto issue_error=ok?ERROR_SUCCESS:GetLastError();
            if(!ok&&issue_error!=ERROR_IO_PENDING){failure={NativeFileReadError::submit,{int(issue_error),std::system_category()}};status.store(NativeFileReadStatus::failed);return;}
            // Covers cancellation requested before the kernel had this request.
            if(cancelling.load())CancelIoEx(file,&work);
            DWORD received=0;
            // Only this owned worker waits. Main/owner poll never waits or joins.
            if(!GetOverlappedResult(file,&work,&received,TRUE)){
                const auto error=GetLastError();
                if(error==ERROR_OPERATION_ABORTED){status.store(NativeFileReadStatus::cancelled);return;}
                failure={NativeFileReadError::read,{int(error),std::system_category()}};status.store(NativeFileReadStatus::failed);return;
            }
            if(received!=issued){failure.kind=NativeFileReadError::short_read;status.store(NativeFileReadStatus::failed);return;}
            done+=received;
        }
        status.store(NativeFileReadStatus::complete);
    }
    static unsigned __stdcall entry(void* argument)noexcept{
        // This independent lease retains buffer/OVERLAPPED/handles through the
        // worker's entire return, even if the owner drops a terminal job.
        std::unique_ptr<std::shared_ptr<State>> lease(static_cast<std::shared_ptr<State>*>(argument));
        (*lease)->run();return 0;
    }
#endif
};
namespace {
bool thread_ok(std::thread::id owner,NativeFileReadFailure& f)noexcept{f={};if(owner==std::this_thread::get_id())return true;f.kind=NativeFileReadError::wrong_thread;return false;}
}
NativeFileReadJob::NativeFileReadJob(std::shared_ptr<State> s):state_(std::move(s)){}
NativeFileReadJob::~NativeFileReadJob(){
#ifdef _WIN32
    if(state_->worker&&state_->status.load()==NativeFileReadStatus::pending){std::fputs("native read job owner dropped pending work\n",stderr);std::terminate();}
#endif
}
std::unique_ptr<NativeFileReadJob> NativeFileReadJob::begin(NativeFile& source,std::uint64_t offset,std::size_t count,NativeFileReadFailure& f)noexcept{
    f={};if(!source.file_){f.kind=NativeFileReadError::closed;return {};}
#ifndef _WIN32
    (void)offset;(void)count;f.kind=NativeFileReadError::unsupported_host;return {};
#else
    if(count>std::numeric_limits<std::uint64_t>::max()-offset){f.kind=NativeFileReadError::invalid_range;return {};}
    try{
        auto state=std::make_shared<State>();state->offset=offset;
        const auto raw=_get_osfhandle(_fileno(source.file_));if(raw==-1||raw==-2){f.kind=NativeFileReadError::closed;return {};}
        state->file=ReOpenFile(reinterpret_cast<HANDLE>(raw),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,FILE_FLAG_OVERLAPPED);
        if(state->file==INVALID_HANDLE_VALUE){f={NativeFileReadError::open,{int(GetLastError()),std::system_category()}};return {};}
        LARGE_INTEGER size{};
        if(GetFileType(state->file)!=FILE_TYPE_DISK||!GetFileSizeEx(state->file,&size)||size.QuadPart<0||offset>std::uint64_t(size.QuadPart)||count>std::uint64_t(size.QuadPart)-offset){f.kind=NativeFileReadError::invalid_range;return {};}
        state->data.resize(count);
        state->event=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!state->event){f={NativeFileReadError::event,{int(GetLastError()),std::system_category()}};return {};}
        // Allocate the public owner BEFORE IO can retain its buffer.
        auto job=std::unique_ptr<NativeFileReadJob>(new NativeFileReadJob(std::move(state)));
        if(!count)job->state_->status.store(NativeFileReadStatus::complete);
        else{
            auto lease=std::make_unique<std::shared_ptr<State>>(job->state_);
            job->state_->worker=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,State::entry,lease.get(),0,nullptr));
            if(!job->state_->worker){f={NativeFileReadError::worker,{errno?errno:EAGAIN,std::generic_category()}};return {};}
            lease.release();
        }
        return job;
    }catch(const std::bad_alloc&){f.kind=NativeFileReadError::allocation;}
     catch(const std::length_error&){f.kind=NativeFileReadError::host_capacity;}
    return {};
#endif
}
std::unique_ptr<NativeFileReadJob> NativeFileReadJob::begin(NativeContentFile& file,std::uint64_t offset,std::size_t count,NativeFileReadFailure& f)noexcept{
    if(!file.valid()){f={NativeFileReadError::closed,{}};return {};}
    auto job=begin(file.file_,offset,count,f);if(job)job->state_->volume=file.volume_;return job;
}
std::optional<NativeFileReadStatus> NativeFileReadJob::poll(NativeFileReadFailure& f)noexcept{
    auto& s=*state_;if(!thread_ok(s.owner,f))return {};
    const auto status=s.status.load();if(status==NativeFileReadStatus::failed)f=s.failure;return status;
}
bool NativeFileReadJob::cancel(NativeFileReadFailure& f)noexcept{
    auto& s=*state_;if(!thread_ok(s.owner,f))return false;
#ifdef _WIN32
    if(s.status.load()!=NativeFileReadStatus::pending)return true;
    s.cancelling.store(true);
    if(!CancelIoEx(s.file,&s.work)){const auto error=GetLastError();if(error!=ERROR_NOT_FOUND){f={NativeFileReadError::cancel,{int(error),std::system_category()}};return false;}}
    return true; // Completion still must be observed.
#else
    f.kind=NativeFileReadError::unsupported_host;return false;
#endif
}
std::optional<std::span<const std::uint8_t>> NativeFileReadJob::bytes(NativeFileReadFailure& f)const noexcept{
    const auto& s=*state_;if(!thread_ok(s.owner,f))return {};
    if(s.status.load()!=NativeFileReadStatus::complete){f.kind=NativeFileReadError::not_complete;return {};}
    return std::span<const std::uint8_t>(s.data);
}
}
