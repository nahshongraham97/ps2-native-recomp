#include "mka/movie_source_syntax_index.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <new>
#include <stdexcept>
using namespace mka::video;
namespace {int allocation_after=-1;}
void* operator new(std::size_t n){if(allocation_after==0){allocation_after=-1;throw std::bad_alloc();}if(allocation_after>0)--allocation_after;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
using Bytes=std::vector<std::uint8_t>;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
struct Bits {
    Bytes bytes;unsigned bit=0;
    explicit Bits(unsigned code):bytes{0,0,1,std::uint8_t(code)}{}
    void put(unsigned n,std::uint32_t v){for(unsigned i=0;i<n;++i){if(!bit)bytes.push_back(0);bytes.back()|=((v>>(n-1-i))&1u)<<(7-bit);bit=(bit+1)%8;}}
};
Bytes sequence(bool matrix=false){Bits b(0xb3);b.put(12,640);b.put(12,448);b.put(4,3);b.put(4,4);b.put(18,12345);b.put(1,1);b.put(10,112);b.put(1,0);b.put(1,matrix);if(matrix)for(unsigned i=0;i<64;++i)b.put(8,i);b.put(1,0);if(matrix){b.bytes[20]=0;b.bytes[21]=0;b.bytes[22]=1;b.bytes[23]=0xb8;}return b.bytes;}
Bytes ext(){Bits b(0xb5);b.put(4,1);b.put(8,0x48);b.put(1,1);b.put(2,1);b.put(2,0);b.put(2,0);b.put(12,0);b.put(1,1);b.put(8,0);b.put(1,0);b.put(2,0);b.put(5,0);return b.bytes;}
Bytes coding(unsigned structure=3,bool composite=false){Bits b(0xb5);b.put(4,8);for(unsigned i=0;i<4;++i)b.put(4,1);b.put(2,0);b.put(2,structure);for(unsigned i=0;i<9;++i)b.put(1,i==8);b.put(1,composite);if(composite)b.put(20,0xabcde);return b.bytes;}
Bytes picture(unsigned ref=0,unsigned type=1){Bits b(0);b.put(10,ref);b.put(3,type);b.put(16,0xffff);if(type>1){b.put(1,0);b.put(3,1);}if(type==3){b.put(1,0);b.put(3,1);}b.put(1,0);return b.bytes;}
Bytes gop(){Bits b(0xb8);b.put(25,1u<<12);b.put(1,1);b.put(1,0);return b.bytes;}
Bytes quant(){Bits b(0xb5);b.put(4,3);for(unsigned k=0;k<4;++k){b.put(1,k==2);if(k==2)for(unsigned i=0;i<64;++i)b.put(8,i);}return b.bytes;}
void add(Bytes& out,const Bytes& b){out.insert(out.end(),b.begin(),b.end());}
void packet(Bytes& out,std::span<const std::uint8_t> bytes,bool pts){const std::size_t h=pts?5:1,n=h+bytes.size();check(n<=65535,"synthetic packet length");add(out,{0,0,1,0xe0,std::uint8_t(n>>8),std::uint8_t(n)});if(pts)add(out,{0x21,0,1,0,0x21});else out.push_back(15);out.insert(out.end(),bytes.begin(),bytes.end());}
struct Fixture {
    std::filesystem::path parent,root;unsigned serial=0;
    Fixture(){parent=std::filesystem::canonical(std::filesystem::temp_directory_path());root=parent/("mka-syntax-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));check(std::filesystem::create_directory(root),"exclusive fixture directory");root=std::filesystem::canonical(root);check(root.parent_path()==parent,"canonical fixture boundary");}
    ~Fixture(){std::error_code e;auto p=std::filesystem::canonical(root,e);if(!e&&p.is_absolute()&&p==root&&p.parent_path()==parent)std::filesystem::remove_all(p,e);}
    NativeMoviePesIndex index(const Bytes& es,std::size_t split=0){
        Bytes ps{0,0,1,0xba,0x21,0,1,0,1,0x80,0,1};
        if(split){packet(ps,std::span(es).first(split),false);packet(ps,std::span(es).subspan(split),true);}else packet(ps,es,true);
        add(ps,{0,0,1,0xb9,255});const auto name="syntax-"+std::to_string(serial++)+".sfd";
        {std::ofstream f(root/name,std::ios::binary);f.write(reinterpret_cast<const char*>(ps.data()),ps.size());check(bool(f),"synthetic PS write");}
        MovieAssetFailure af;auto store=NativeMovieAssetStore::create(root,{{"source",name}},af);check(bool(store),"fixture store");auto asset=store->open("source",af);check(bool(asset),"fixture native opened asset");MoviePesFailure pf;auto index=NativeMoviePesIndex::build(std::move(*asset),pf);check(bool(index),"synthetic indexed PS");return std::move(*index);
    }
};
}
int main(){try{
    Fixture fixture;auto identity=std::make_shared<MovieSourceObservationIdentity>();MovieSourceSyntaxFailure f;
    Bytes es=sequence(true);add(es,ext());add(es,quant());const auto first=es.size();add(es,picture(2));add(es,coding(1,true));
    const Bytes b2{0,0,1,0xb2,0x80,0xff};add(es,b2);add(es,{0,0,1,1,0x11});
    const auto second=es.size();add(es,picture(0,3));add(es,coding());add(es,{0,0,1,2,0x55});add(es,{0,0,1,0xb7});
    auto pes=fixture.index(es,first+2);auto index=NativeMovieSourceSyntaxIndex::build(pes,0xe0,"source",identity,f);
    check(index&&index->pictures().size()==2,"embedded matrix prefix skipped and two format pictures");
    auto held=index->lookup("source",identity,first,f);check(held&&held->picture.temporal_reference==2&&!held->gop&&!held->quarter_pts&&held->pes.es_end==first+2,"cross-PES prefix owns absent first-byte PTS");
    check(held->sequence.width==640&&held->sequence.height==448&&held->sequence.intra_matrix&&held->coding.picture_structure==1&&held->coding.composite_display&&held->coding.composite_display->sub_carrier_phase==0xde,"all serialized fields retained without field-output claim");
    check(held->userdata.size()==1&&held->userdata[0].unit.bytes==b2&&std::get<MovieUserDataHeader>(held->userdata[0].unit.payload).payload==Bytes({0x80,0xff}),"whole B2 including binary payload belongs format picture");
    check(held->quant_history_end-held->quant_history_begin==1&&std::get<MovieQuantMatrixExtension>((*held->quant_matrix_units)[held->quant_history_begin].unit.payload).chroma_intra_matrix.has_value(),"ID3 flat immutable history retained");
    auto other=index->lookup("source",identity,second,f);check(other&&other->quarter_pts==4&&other->picture.coding_type==3&&other->userdata.empty(),"B picture own source and no stale userdata");
    check(!index->lookup("source",identity,first+1,f)&&f.kind==MovieSourceSyntaxError::range,"exact offset only");
    auto foreign=std::make_shared<MovieSourceObservationIdentity>();check(!index->lookup("source",foreign,first,f)&&f.kind==MovieSourceSyntaxError::foreign_source,"foreign same key rejected");check(!index->lookup("other",identity,first,f)&&f.kind==MovieSourceSyntaxError::foreign_source,"foreign key rejected");
    std::optional<NativeMovieSourceSyntaxIndex> moved(std::move(*index));check(!index->lookup("source",identity,first,f)&&f.kind==MovieSourceSyntaxError::invalid_owner,"moved index rejects");*moved=std::move(*moved);check(moved->lookup("source",identity,first,f)==held,"self move preserves exact record");index.reset();moved.reset();pes=fixture.index({0,0,1,0xb7});es.clear();check(held->userdata[0].unit.bytes==b2&&held->quant_matrix_units->size()==1&&held->source_key=="source","immutable record survives every original index/file/input owner");
    Bytes reset=sequence();add(reset,ext());add(reset,gop());auto a=reset.size();add(reset,picture());add(reset,coding());add(reset,{0,0,1,1,8});add(reset,{0,0,1,0xb7});add(reset,sequence());add(reset,ext());auto b=reset.size();add(reset,picture());add(reset,coding());add(reset,{0,0,1,1,9});
    auto reset_pes=fixture.index(reset);auto reset_index=NativeMovieSourceSyntaxIndex::build(reset_pes,0xe0,"source",identity,f);check(reset_index&&reset_index->lookup("source",identity,a,f)->gop&& !reset_index->lookup("source",identity,b,f)->gop,"B7 and new sequence reset GOP absence");
    auto reject=[&](Bytes raw,MovieSourceSyntaxError expected){auto source=fixture.index(raw);auto bad=NativeMovieSourceSyntaxIndex::build(source,0xe0,"source",identity,f);check(!bad&&f.kind==expected,"named malformed/incomplete source rejection");};
    reject(picture(),MovieSourceSyntaxError::missing_sequence);
    Bytes missing=sequence();add(missing,picture());reject(missing,MovieSourceSyntaxError::missing_sequence_extension);
    missing=sequence();add(missing,ext());add(missing,picture());reject(missing,MovieSourceSyntaxError::missing_coding_extension);
    add(missing,coding());reject(missing,MovieSourceSyntaxError::missing_slice);
    missing=sequence();add(missing,ext());add(missing,{0,0,1,0xb5,0x40});reject(missing,MovieSourceSyntaxError::unsupported);
    missing=sequence();add(missing,ext());add(missing,{0,0,1,0});reject(missing,MovieSourceSyntaxError::need_more);
    check(!NativeMovieSourceSyntaxIndex::build(reset_pes,0xc0,"source",identity,f)&&f.kind==MovieSourceSyntaxError::invalid_stream,"audio cannot become syntax video");
    check(!NativeMovieSourceSyntaxIndex::build(reset_pes,0xe0,"",identity,f)&&f.kind==MovieSourceSyntaxError::invalid_source,"source identity inputs mandatory");
    allocation_after=0;auto failed=NativeMovieSourceSyntaxIndex::build(reset_pes,0xe0,"source",identity,f);allocation_after=-1;check(!failed&&f.kind==MovieSourceSyntaxError::allocation,"genuine allocation fault names unpublished index");check(NativeMovieSourceSyntaxIndex::build(reset_pes,0xe0,"source",identity,f).has_value(),"source reusable after allocation failure");
    auto moved_pes=std::move(reset_pes);check(!NativeMovieSourceSyntaxIndex::build(reset_pes,0xe0,"source",identity,f)&&f.kind==MovieSourceSyntaxError::invalid_owner,"moved source index rejected by retained reader validation");reset_pes=std::move(moved_pes);
    unsigned faulted=0;for(int point=0;;++point){allocation_after=point;auto attempt=NativeMovieSourceSyntaxIndex::build(reset_pes,0xe0,"source",identity,f);allocation_after=-1;if(attempt)break;check(f.kind==MovieSourceSyntaxError::allocation,"every actual build allocation stage reports allocation without partial owner");++faulted;}check(faulted>=4,"fault injection reaches ES/state/record allocations");
    Bytes large=sequence();add(large,ext());for(unsigned i=0;i<300;++i){add(large,picture(i));add(large,coding());add(large,{0,0,1,1,0x55});}auto large_pes=fixture.index(large);auto large_index=NativeMovieSourceSyntaxIndex::build(large_pes,0xe0,"source",identity,f);check(large_index&&large_index->pictures().size()==300,"dynamic picture storage beyond fixed record pools");
    std::puts("PASS immutable source syntax pictures; no SDK admission or selected-output association");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL source syntax index %s\n",e.what());return 1;}}
