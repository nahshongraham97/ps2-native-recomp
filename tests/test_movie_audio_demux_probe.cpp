// Independent demux/parser observations; no project media/runtime or audio codec API calls.
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>
}
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <fstream>
#include <set>
#include <optional>
#include <cerrno>
#include <algorithm>
#include <cstring>

namespace {
void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
struct Api {
    HMODULE u=nullptr,c=nullptr,f=nullptr;
#define DECL(name) decltype(&name) name##_p=nullptr
    DECL(av_malloc); DECL(av_free); DECL(av_strdup); DECL(avio_alloc_context); DECL(avio_context_free); DECL(avformat_alloc_context); DECL(avutil_version); DECL(avcodec_version); DECL(avformat_version);
    DECL(avformat_open_input); DECL(avformat_find_stream_info); DECL(avformat_close_input);
    DECL(av_packet_alloc); DECL(av_packet_unref); DECL(av_packet_free); DECL(av_read_frame);
#undef DECL
    explicit Api(const std::filesystem::path& dir){try{
        const auto load=[&](const wchar_t* name){auto m=LoadLibraryExW((dir/name).c_str(),nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);check(m!=nullptr,"load pinned codec DLL");return m;};
        u=load(L"avutil-59.dll");c=load(L"avcodec-61.dll");f=load(L"avformat-61.dll");
#define LOAD(module,name) name##_p=reinterpret_cast<decltype(name##_p)>(GetProcAddress(module,#name));check(name##_p!=nullptr,#name)
        LOAD(u,av_malloc);LOAD(u,av_free);LOAD(u,av_strdup);LOAD(f,avio_alloc_context);LOAD(f,avio_context_free);LOAD(f,avformat_alloc_context);LOAD(u,avutil_version);LOAD(c,avcodec_version);LOAD(f,avformat_version);
        LOAD(f,avformat_open_input);LOAD(f,avformat_find_stream_info);LOAD(f,avformat_close_input);
        LOAD(c,av_packet_alloc);LOAD(c,av_packet_unref);LOAD(c,av_packet_free);LOAD(f,av_read_frame);
#undef LOAD
        check((avutil_version_p()>>16)==LIBAVUTIL_VERSION_MAJOR,"utility ABI");
        check((avcodec_version_p()>>16)==LIBAVCODEC_VERSION_MAJOR,"codec ABI");
        check((avformat_version_p()>>16)==LIBAVFORMAT_VERSION_MAJOR,"format ABI");
    }catch(...){if(f)FreeLibrary(f);if(c)FreeLibrary(c);if(u)FreeLibrary(u);throw;}
    }
    ~Api(){if(f)FreeLibrary(f);if(c)FreeLibrary(c);if(u)FreeLibrary(u);}
};
struct Hash {
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;std::vector<UCHAR> object;
    Hash(){try{check(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"SHA256 provider");
        DWORD size=0,got=0;check(BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof size,&got,0)==0,"SHA256 size");
        object.resize(size);check(BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0)==0,"SHA256 create");}catch(...){if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);throw;}}
    ~Hash(){if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);}
    void add(const std::uint8_t* p,unsigned n){check(BCryptHashData(hash,const_cast<PUCHAR>(p),n,0)==0,"SHA256 update");}
    std::string finish(){std::array<UCHAR,32>b{};check(BCryptFinishHash(hash,b.data(),b.size(),0)==0,"SHA256 finish");
        constexpr char h[]="0123456789abcdef";std::string out;for(auto c:b){out+=h[c>>4];out+=h[c&15];}return out;}
};
void add(std::uint64_t& n,std::uint64_t amount){check(amount<=std::numeric_limits<std::uint64_t>::max()-n,"native count overflow");n+=amount;}
void quote(std::string_view s){std::cout<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')std::cout<<'\\'<<c;
    else if(c<32){constexpr char h[]="0123456789abcdef";std::cout<<"\\u00"<<h[c>>4]<<h[c&15];}else std::cout<<c;}std::cout<<'"';}

struct Input {
    Api& api;std::ifstream file;std::uint64_t length=0;bool io_error=false;
    AVFormatContext* format=nullptr;AVIOContext* io=nullptr;AVPacket* packet=nullptr;
    explicit Input(Api& a,const std::filesystem::path& path):api(a),file(path,std::ios::binary){
        check(bool(file),"native input open");file.seekg(0,std::ios::end);auto end=file.tellg();check(end>=0,"native input length");length=static_cast<std::uint64_t>(end);file.seekg(0);check(bool(file),"native input rewind");
    }
    static int read(void* opaque,std::uint8_t* dst,int size)noexcept{
        auto& s=*static_cast<Input*>(opaque);if(size<=0)return AVERROR(EINVAL);
        s.file.read(reinterpret_cast<char*>(dst),size);const auto n=s.file.gcount();
        if(s.file.bad()||(!s.file.eof()&&s.file.fail())){s.io_error=true;return AVERROR(EIO);}
        if(n>0)return static_cast<int>(n);return AVERROR_EOF;
    }
    static std::int64_t seek(void* opaque,std::int64_t offset,int whence)noexcept{
        auto& s=*static_cast<Input*>(opaque);whence&=~AVSEEK_FORCE;
        if(whence==AVSEEK_SIZE)return s.length<=INT64_MAX?static_cast<std::int64_t>(s.length):AVERROR(EOVERFLOW);
        std::int64_t base=0;
        if(whence==SEEK_CUR){s.file.clear();auto pos=s.file.tellg();if(pos<0){s.io_error=true;return AVERROR(EIO);}base=static_cast<std::int64_t>(pos);}
        else if(whence==SEEK_END){if(s.length>INT64_MAX)return AVERROR(EOVERFLOW);base=static_cast<std::int64_t>(s.length);}
        else if(whence!=SEEK_SET)return AVERROR(EINVAL);
        if((offset>0&&base>INT64_MAX-offset)||(offset<0&&base<INT64_MIN-offset))return AVERROR(EOVERFLOW);
        const auto target=base+offset;if(target<0||static_cast<std::uint64_t>(target)>s.length)return AVERROR(EINVAL);
        s.file.clear();s.file.seekg(target);if(!s.file){s.io_error=true;return AVERROR(EIO);}return target;
    }
    void open(){
        auto* b=static_cast<unsigned char*>(api.av_malloc_p(65536));check(b,"AVIO allocation");
        io=api.avio_alloc_context_p(b,65536,0,this,read,nullptr,seek);if(!io){api.av_free_p(b);throw std::runtime_error("AVIO context allocation");}
        format=api.avformat_alloc_context_p();check(format,"format allocation");format->pb=io;format->flags|=AVFMT_FLAG_CUSTOM_IO;
        // Owned by AVFormatContext; all internal open sites forward this whitelist.
        format->codec_whitelist=api.av_strdup_p("mpeg2video");check(format->codec_whitelist,"codec whitelist allocation");
        check(api.avformat_open_input_p(&format,nullptr,nullptr,nullptr)>=0,"custom AVIO demux open");
        check(format->codec_whitelist&&std::strcmp(format->codec_whitelist,"mpeg2video")==0,"required codec whitelist preserved");
        check(api.avformat_find_stream_info_p(format,nullptr)>=0,"MPEG2-only stream-info probing");
        check(!io_error,"native AVIO probing error");packet=api.av_packet_alloc_p();check(packet,"packet allocation");
    }
    ~Input(){if(packet)api.av_packet_free_p(&packet);if(format)api.avformat_close_input_p(&format);if(io){api.av_free_p(io->buffer);io->buffer=nullptr;api.avio_context_free_p(&io);}}
};
std::string time_value(std::int64_t value){return value==AV_NOPTS_VALUE?"null":std::to_string(value);}
std::string bytes_hex(const std::uint8_t* p,std::size_t n){constexpr char h[]="0123456789abcdef";std::string s;check(n<=s.max_size()/2,"hex allocation extent");s.reserve(n*2);for(std::size_t i=0;i<n;++i){s+=h[p[i]>>4];s+=h[p[i]&15];}return s;}
struct Entry{std::string key,path;};
std::vector<Entry> catalog(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary);check(bool(f),"catalog open");std::string line;std::set<std::string>keys,paths;std::vector<Entry> out;
    while(std::getline(f,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();auto t=line.find('\t');check(t>0&&t!=std::string::npos&&t+1<line.size()&&line.find('\t',t+1)==std::string::npos,"strict catalog row");Entry e{line.substr(0,t),line.substr(t+1)};check(e.key.find('\0')==std::string::npos&&e.path.find('\0')==std::string::npos,"catalog NUL");auto relative=std::filesystem::u8path(e.path);check(!relative.is_absolute()&&!relative.has_root_name()&&!relative.has_root_directory(),"catalog relative path");for(const auto& c:relative)check(c!=".."&&c!=".","catalog traversal");check(keys.insert(e.key).second&&paths.insert(e.path).second,"catalog duplicate");out.push_back(std::move(e));}
    check(f.eof()&&!f.bad()&&out.size()==38,"complete38 catalog");return out;
}
void observe(Api& api,const std::filesystem::path& root,const Entry& entry,const std::filesystem::path& metadata_directory,const std::string& metadata_name,bool& has_audio){
    std::ofstream artifact(metadata_directory/std::filesystem::u8path(metadata_name),std::ios::binary|std::ios::out);check(bool(artifact),"packet metadata artifact open");Hash artifact_hash;std::uint64_t artifact_bytes=0;
    Input input(api,root/std::filesystem::u8path(entry.path));input.open();int selected=-1;
    for(unsigned i=0;i<input.format->nb_streams;++i){const auto* s=input.format->streams[i];if(s->codecpar->codec_type!=AVMEDIA_TYPE_AUDIO)continue;check(s->codecpar->codec_id==AV_CODEC_ID_ADPCM_ADX,"unsupported audio codec");check(selected<0,"multiple ADX streams unsupported");check(s->id>=0x1c0&&s->id<=0x1df,"unsupported ADX full stream id");selected=static_cast<int>(i);}
    const AVStream* stream=selected<0?nullptr:input.format->streams[selected];has_audio=stream!=nullptr;
    Hash packets,whole,metadata;std::string extra;std::uint64_t es=0,packet_bytes=0,count=0,missing_pts=0,missing_dts=0;std::map<std::uint64_t,std::uint64_t> histogram;std::string first,last;
    if(stream){check(stream->codecpar->extradata_size>=0,"negative extradata");auto n=static_cast<unsigned>(stream->codecpar->extradata_size);check(!n||stream->codecpar->extradata,"null extradata");extra=bytes_hex(stream->codecpar->extradata,n);if(n)whole.add(stream->codecpar->extradata,n);es=n;check(stream->time_base.den>0,"invalid stream timebase");}
    const std::uint8_t left='[',right=']',comma=',';metadata.add(&left,1);int status=0;
    while((status=api.av_read_frame_p(input.format,input.packet))>=0){auto& p=*input.packet;
        if(p.stream_index==selected){check(p.size>0&&p.data,"invalid audio packet extent");check(!(p.flags&AV_PKT_FLAG_CORRUPT),"corrupt audio packet");check(count<UINT64_MAX,"packet sequence overflow");const auto begin=es;add(es,static_cast<unsigned>(p.size));const auto row="["+std::to_string(count+1)+","+std::to_string(begin)+","+std::to_string(es)+","+time_value(p.pts)+","+time_value(p.dts)+","+std::to_string(stream->time_base.num)+","+std::to_string(stream->time_base.den)+"]";
            const auto artifact_line=row+"\n";artifact.write(artifact_line.data(),static_cast<std::streamsize>(artifact_line.size()));check(bool(artifact),"packet metadata artifact write");artifact_hash.add(reinterpret_cast<const std::uint8_t*>(artifact_line.data()),static_cast<unsigned>(artifact_line.size()));add(artifact_bytes,artifact_line.size());
            if(count)metadata.add(&comma,1);metadata.add(reinterpret_cast<const std::uint8_t*>(row.data()),static_cast<unsigned>(row.size()));if(!count)first=row;last=row;
            packets.add(p.data,static_cast<unsigned>(p.size));whole.add(p.data,static_cast<unsigned>(p.size));add(packet_bytes,static_cast<unsigned>(p.size));add(count,1);add(histogram[static_cast<unsigned>(p.size)],1);if(p.pts==AV_NOPTS_VALUE)add(missing_pts,1);if(p.dts==AV_NOPTS_VALUE)add(missing_dts,1);
        }input.api.av_packet_unref_p(input.packet);
    }
    check(status==AVERROR_EOF&&!input.io_error,"demux EOF or native IO error");metadata.add(&right,1);if(stream)check(count>0,"ADX stream without packets");
    artifact.flush();check(bool(artifact),"packet metadata artifact flush");artifact.close();check(!artifact.fail(),"packet metadata artifact close");
    std::cout<<"{\"key\":";quote(entry.key);std::cout<<",\"audio_present\":"<<(stream?"true":"false")<<",\"probe_codec_whitelist\":\"mpeg2video\",\"audio_stream_index\":"<<(stream?std::to_string(selected):"null")<<",\"audio_stream_id\":"<<(stream?std::to_string(stream->id):"null")<<",\"time_base\":";
    if(stream)std::cout<<'['<<stream->time_base.num<<','<<stream->time_base.den<<']';else std::cout<<"null";
    std::cout<<",\"extradata_hex\":";quote(extra);std::cout<<",\"packet_metadata_file\":";quote(metadata_name);std::cout<<",\"packet_metadata_jsonl_sha256\":";quote(artifact_hash.finish());std::cout<<",\"packet_metadata_jsonl_bytes\":"<<artifact_bytes;std::cout<<",\"packet_count\":"<<count<<",\"packet_bytes\":"<<packet_bytes<<",\"es_bytes\":"<<es<<",\"packet_bytes_sha256\":";quote(packets.finish());std::cout<<",\"source_es_sha256\":";quote(whole.finish());std::cout<<",\"packet_metadata_sha256\":";quote(metadata.finish());std::cout<<",\"missing_pts\":"<<missing_pts<<",\"missing_dts\":"<<missing_dts<<",\"first_packet\":"<<(first.empty()?"null":first)<<",\"last_packet\":"<<(last.empty()?"null":last)<<",\"packet_length_histogram\":[";
    bool c=false;for(const auto& [length,n]:histogram){if(c)std::cout<<',';c=true;std::cout<<'['<<length<<','<<n<<']';}std::cout<<"]}\n"<<std::flush;check(bool(std::cout),"output write");
}
}
int main(int argc,char** argv){try{
    check(argc==5,"usage: test_movie_audio_demux_probe DLLdir asset_root catalog.tsv NEW_metadata_dir");auto entries=catalog(std::filesystem::u8path(argv[3]));const auto metadata_directory=std::filesystem::absolute(std::filesystem::u8path(argv[4]));std::error_code directory_error;check(std::filesystem::create_directory(metadata_directory,directory_error)&&!directory_error,"packet metadata directory must be newly created");Api api(std::filesystem::u8path(argv[1]));std::size_t audio=0,errors=0,ordinal=0;
    for(const auto& e:entries){char filename[64];check(ordinal<38,"artifact ordinal bounds");std::snprintf(filename,sizeof filename,"asset-%03u.packets.jsonl",static_cast<unsigned>(ordinal++));try{bool present=false;observe(api,std::filesystem::u8path(argv[2]),e,metadata_directory,filename,present);audio+=present;}catch(const std::exception& ex){++errors;std::cout<<"{\"key\":";quote(e.key);std::cout<<",\"error\":";quote(ex.what());std::cout<<"}\n"<<std::flush;}}
    check(!errors,"one or more asset demux failures");check(audio==29,"expected29 audio and9 absent");std::cout<<"PASS independent audio demux38 ADX29 video-only9; MPEG2-only probing no audio decode\n"<<std::flush;check(bool(std::cout),"completion write");return 0;
}catch(const std::exception& e){std::cerr<<"audio-demux-probe: "<<e.what()<<'\n';return 1;}}
