#include "mka/movie_metadata.hpp"
#include "mka/movie_subtitles.hpp"
#include "native_test_sha256.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace mka::video;
using Bytes=std::vector<std::uint8_t>;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static std::string sha(std::span<const std::uint8_t> b){NativeTestSha256 h;h.add(b.data(),b.size());return h.finish();}
static Bytes tag(const char* name,const Bytes& body){Bytes b(16);std::memcpy(b.data(),name,7);char len[9];std::snprintf(len,sizeof(len),"%07X",unsigned(body.size()));std::memcpy(b.data()+8,len,7);b.insert(b.end(),body.begin(),body.end());return b;}
static MoviePrivate2Payload packet(std::uint64_t offset,Bytes b){MoviePrivate2Payload p;p.packet_offset=offset;p.payload_offset=offset+6;p.bytes=std::move(b);return p;}
static MoviePrivate2Payload header(std::uint64_t start,std::size_t extra=0){
 auto p=packet(start+12,Bytes(2030+extra));std::memcpy(p.bytes.data()+14,"SofdecStream            ",24);std::memcpy(p.bytes.data()+78,"SFM Ver.2.21 ",13);
 MoviePrivate2Prefix x;x.file_offset=start;
 // Independently valid MPEG1 pack SCR=0,muxrate=0 with all marker bits.
 x.bytes={0,0,1,0xba,0x21,0,1,0,1,0x80,0,1,0,0,1,0xbf,0,0};
 x.bytes[16]=std::uint8_t(p.bytes.size()>>8);x.bytes[17]=std::uint8_t(p.bytes.size());p.adjacent_mpeg1_prefix=x;return p;
}
static NativeMovieMetadata assemble(std::vector<MoviePrivate2Payload> p,bool latch=false){MovieMetadataFailure f;auto m=NativeMovieMetadata::assemble(std::move(p),latch,f);if(!m)throw std::runtime_error(movie_metadata_error_name(f.kind));return std::move(*m);}
static void synthetic(){
 auto m=assemble({});check(m.valid()&&m.selection()==MovieMetadataSelection::empty&&!m.next_first_header_latched(),"empty valid absence");
 auto unknown=tag("OPAQUE!",Bytes{1,2,3});m=assemble({packet(0,unknown)});check(m.selection()==MovieMetadataSelection::no_root&&m.ordinary_stream().size()==19,"opaque bounded TLV valid absence");
 auto end=tag("CRITAGE",{});auto root=tag("CRITAGS",Bytes{8,9});auto after=end;after.insert(after.end(),root.begin(),root.end());
 m=assemble({packet(0,after)});check(m.selection()==MovieMetadataSelection::end_before_root&&m.selected_root_bytes().empty()&&m.ordinary_stream().size()==after.size(),"preceding endtag stops without dropping later bytes");
 auto stream=unknown;stream.insert(stream.end(),root.begin(),root.end());stream.push_back(0xff);
 auto first=Bytes(stream.begin(),stream.begin()+23),second=Bytes(stream.begin()+23,stream.end());
 m=assemble({header(0),packet(4096,first),header(8192,99),packet(12288,second)});
 check(m.sofdec_headers().size()==2&&m.origins().size()==2&&m.selection()==MovieMetadataSelection::root_found,"multiple headers and split metadata continuation");
 check(m.ordinary_stream().size()==stream.size()&&std::equal(stream.begin(),stream.end(),m.ordinary_stream().begin()),"whole ordinary stream retained including opaque suffix");
 check(std::equal(root.begin(),root.end(),m.selected_root_bytes().begin(),m.selected_root_bytes().end()),"source TLV target selected without blind substring search");
 check(m.sofdec_headers()[0].first_header_forward_private_index0&&!m.sofdec_headers()[1].first_header_forward_private_index0&&m.next_first_header_latched(),"explicit next latch and first forwarding intent");
 check(m.sofdec_headers()[1].full_bytes.size()==2147&&m.sofdec_headers()[1].callback_intended,"full oversized header retained with conditional callback opportunity");
 auto latched=assemble({header(0)},true);check(!latched.sofdec_headers()[0].first_header_forward_private_index0&&latched.sofdec_headers()[0].source_first_cache_bytes==0,"initial latch respected");
 auto copy=m;auto moved=std::move(m);check(!m.valid()&&m.selection()==MovieMetadataSelection::invalid_owner&&copy.selected_root_bytes().size()==18&&moved.ordinary_stream().size()==stream.size(),"immutable copy/move lifetime");
 MovieMetadataFailure f;const auto rejects=[&](std::vector<MoviePrivate2Payload> p,MovieMetadataError e){auto v=NativeMovieMetadata::assemble(std::move(p),false,f);check(!v&&f.kind==e,"named transactional malformed rejection");};
 auto h=header(0);h.adjacent_mpeg1_prefix.reset();rejects({h},MovieMetadataError::missing_header_prefix);
 h=header(0);h.adjacent_mpeg1_prefix->bytes[4]=0;rejects({h},MovieMetadataError::invalid_header_prefix);
 h=header(0);h.bytes[14+23]='X';rejects({h},MovieMetadataError::invalid_tag_length);
 h=header(0);std::fill(h.bytes.begin()+78,h.bytes.begin()+110,0);rejects({h},MovieMetadataError::malformed_version);
 h=header(0);std::fill(h.bytes.begin()+78,h.bytes.begin()+110,0);std::memcpy(h.bytes.data()+78,"Ver.18446744073709551616.1",26);rejects({h},MovieMetadataError::version_overflow);
 auto bad=tag("CRITAGS",{});bad[8]='x';rejects({packet(0,bad)},MovieMetadataError::invalid_tag_length);
 bad=tag("CRITAGS",{});bad[14]='1';rejects({packet(0,bad)},MovieMetadataError::tag_body_out_of_bounds);
 rejects({packet(0,Bytes(15))},MovieMetadataError::truncated_tag_header);
 rejects({packet(100,unknown),packet(0,unknown)},MovieMetadataError::packet_order);
 // Public records may be inconsistent even if Container never emits them:
 // BF26 follows priorpayloadend26, but its retained BA starts14 inside it.
 rejects({packet(0,tag("CRITAGS",Bytes{1,2,3,4})),header(14)},MovieMetadataError::packet_order);
 auto overflow=packet(0,{});overflow.packet_offset=UINT64_MAX;rejects({overflow},MovieMetadataError::invalid_packet_extent);
 overflow=packet(0,{});overflow.payload_offset=UINT64_MAX;rejects({overflow},MovieMetadataError::invalid_packet_extent);
 // Header-like bytes inside an ordinary TLV BODY are opaque, not scanned.
 auto opaque=tag("OPAQUE!",header(0).bytes);m=assemble({packet(0,opaque)});check(m.sofdec_headers().empty()&&m.selection()==MovieMetadataSelection::no_root,"no arbitrary body marker search");
 // Native storage above32MiB, split only by the serialized uint16 PES length.
 Bytes large(36*1024*1024,0x5a);auto big=tag("OPAQUE!",large);std::vector<MoviePrivate2Payload> packets;
 for(std::size_t at=0;at<big.size();){auto n=std::min<std::size_t>(65535,big.size()-at);packets.push_back(packet(at*2,Bytes(big.begin()+at,big.begin()+at+n)));at+=n;}
 m=assemble(std::move(packets));check(m.selection()==MovieMetadataSelection::no_root&&m.ordinary_stream().size()==big.size(),"no native32MiB/2KiB or packet-count capacity");
}
struct Reference {const char* key;const char* asset_sha;const char* stream_sha;unsigned ordinary;};
// REFERENCE_ROWS: bounded source/independent prior private2 byte inventory.
static const Reference references[]={
    {"ATTRACTN.SFD","4fd676d3541ffea5adbd1ab9484143cbd05916ce85ef63421ab59b78a058fc86","f40f48f1e7ced49dc954fb2082d03e048e6f3eb261b3fdd17999453ebcc88e90",1},
    {"ATTRACTW.SFD","9ddba26fabaf9f1e0219ba1b2a364f55780e73ec78b0b51bacb5e974f4c90ad1","f40f48f1e7ced49dc954fb2082d03e048e6f3eb261b3fdd17999453ebcc88e90",1},
    {"KRYPT/V_BLOOP2.SFD","5a4c60d434d02f0cab6550201c95e16f7fc114ab9a371f884b624987a4372f24","154c93f192125a2b651a4067b1c984a56168bc43c8f5484557a6bb3d1bce8414",1},
    {"KRYPT/V_BLOOP3.SFD","dc9805f3224ab45e6bb35c88b8e54f4057594a3665b93b674083aa36e6db8840","df1c3fb6d29ca5a8a8bd919b1d1ada3ba66c747b42f8950e872cd5ea8a896e0b",1},
    {"KRYPT/V_BLOOP4.SFD","3d4e2ca483a11e9f79def22e0895fe7901b8829c976146966ed4e9933a7fc558","2b2febd589997f7eaa84f7c87df8e39ed2c43cfb061303412cec91a5cdaf0427",1},
    {"KRYPT/V_BLOOP5.SFD","d7a883cfa6f0db93d5da0db5c26e7f80b72ae0643006c35dca28d5dc24ab7274","b47cdb7844ea4cd4a2166565441df1df178bc6ec99045c2d743a8c995976d1c5",1},
    {"KRYPT/V_BLOOPR.SFD","34466ceff507bc367a9817bcd7a0cbd9ac0dc584e60dd07df1affade0d1ebf75","2c346aa299d8728c6fac756583822608cfd1609a6c8e23bc31cd7b17676c89cb",1},
    {"KRYPT/V_BLUES.SFD","1758bf5e4bbc5ba14ed88995796d9004440534e7051691d683bff3cb000a4c8b","15caa0d6c3ac9aab578f46fda1c7c12c9a8d01cbbcf1eb00557fad25fb39500a",1},
    {"KRYPT/V_ERMACB.SFD","24e113180f3fc267e28c3eac6ec31bfeb671708783addc77dc95e22f906a926e","50a113f01e219a76a74e02a73a031121a8877f5dbf5aac3cf1f98cb0493d5190",1},
    {"KRYPT/V_KANO.SFD","3aec43f776bd8b2ed8255684858586d9b1a5aeed513626c43c8494b09cbe8782","b6f3fd17e0d879efd330136d61bdd479e7e6b8da2e5de5e32aaf6759c5591468",1},
    {"KRYPT/V_KONQIN.SFD","9a186deb9c9700d98b54d63a5172e3cb424e6abecef0aa6b3e0649ff00f1e9a6","49d74548ba3b98ae35daa65cf908dddfe508cf0c23c65a45217ee5b540913ad6",1},
    {"KRYPT/V_LOAD1.SFD","d9c655a5225803c887e16ae982950a1b5570eb80b37ec10b7558b1cabd68cb60","cfc485d81e02afbf87c2658031bc09aff6177a3c060bfb4333fa689c45c83df7",1},
    {"KRYPT/V_LOAD2.SFD","2e81b069ced5a0a3bb3921dfa0c10e8e758454b9036df92b68de557c8eaa2e84","e72e20067fcf78e223062f53ebc5553c3bebc0c60380e6334139251df087459a",1},
    {"KRYPT/V_MKAKON.SFD","3e46dd8d7c305782d62cd5e512194dfda3e43eb8552cb985ddd9e8c59724cf25","97991138da558cf840aaed7895b41d02b9ac33c64198da2bbc0e3193116a7274",1},
    {"KRYPT/V_MKDACC.SFD","cce2e8491088726d615a1d750c332a0e90d2ac27931a9e06195d5bb0df1116fb","762c96b0c63a05963195cebcba874c6a15716d67ca1dc45685b90be4345a96f9",1},
    {"KRYPT/V_MKHAND.SFD","fd6f2f9d2d1a901cbe4595b18cb991d175ce5dc575fe59f5d8896ebd11bb9b0c","cffbb364fb0907412e0c628d165b203ee7638d4d7a27de9d0cf3943854e65c06",1},
    {"KRYPT/V_MOTOGI.SFD","26f25af4e4f6d526633952761fb76fafcf5d7a80d19ee1ed92f9f4269ed4bec0","3f8101cdda9fc80be8793eb98d32e104e2e10952ed052b20efb9c81d0fd04f3c",1},
    {"KRYPT/V_MOTORK.SFD","e51cccf5d18acab1506e09aa66f9f940e605544ada23e10297a783e7eb88f091","1c544eda4df66a10879f8e6be4df54be181846a5590c7463859ceac203dc53c2",1},
    {"KRYPT/V_MOTRBK.SFD","142797674920a4a656584a65d0c997b11b748e2ba09eccd6b2a2479e52df5033","f8b44d2c8abc1300600695a7623b3264ac16b7edb4539e3bee4741c9ba792e26",1},
    {"KRYPT/V_MOTRFT.SFD","1a31d7881426dfc5d5e1319b99837d2b00580f06f4b9d94c7cd0d7fbfc2653d9","98d70c57a6349a6ae719f731aff67d629cacd3dfc373a0a6b1984ef00b370eb3",1},
    {"KRYPT/V_RAIDON.SFD","0067d84f92072d88e065f7cbe48b3c89efd74d08094be6894120de7736b11999","b63827b4bd8454861da75740bd34dc0f0d36d7ae72e05e3052d8f7aee2e15127",1},
    {"KRYPT/V_SCORP.SFD","4fac65428df7749456aa1034313b19a024d2a0182fedb1fffa03273ce8e741f7","79fa721ccb2771c1a7c753ce4bff83466f70a6b0c42785302c2c80404e45d7b7",1},
    {"KRYPT/V_SHINOK.SFD","651c135da991113c1d70446a829a459f07ce6d71f46b36aa413746100c310029","850cd7177ccf1eab758c1d7aaafad479baa9cf0cea1cbd757acc9adee378a570",1},
    {"KRYPT/V_SNDOUT.SFD","9a8738c9480a440b803830f18c1754619b1b880acfc40a6f24c5de474b300f31","80513bc18c611e447e9a7064519d6e6ac55ad4a9a41940b87257a4454bd1c4d4",1},
    {"KRYPT/V_SUBZER.SFD","d1dd7751a559afcfe8155c7bfe6eebe772519a2e9e5cfd5dba9cd65de0bafb0a","61698ff4fc7ccc72a9eb0e1aa08a341d59dc8526456704d5bdcadd680f59804b",1},
    {"KRYPT/V_SZCCKY.SFD","731c8c889e8a61446428df4264d780ba03b245558dbabf5b1a73888fc79158de","518a6e716087720d2c5b19c752c6d906e19f960013fb93e0ce81471e43e2338c",1},
    {"LOGON.SFD","cf49fb014d51f20fee7727a01e845cc7303be63fe0af2902fb3eadf371a843af","671243a10e71294737316e2f0aed500aa8f4c502390d5c96c435a4f86f7f601e",1},
    {"LOGOW.SFD","fa5e25182e8c054c3d673ada0ed9d38d6e3b3f2c9ac14bde9d5baa862eb81e71","671243a10e71294737316e2f0aed500aa8f4c502390d5c96c435a4f86f7f601e",1},
    {"OPENINGN.SFD","a2b3e83b23de92c31879dbcc4ab9415e07bbbb0638ea4f90d112f969de3e5e53","46dd710a6e751e96985016ebb313e09a6391bc7b86aadc59dd9c00ecf51b4bcd",1},
    {"OPENINGW.SFD","5c68f06a1f376f58be350c12e6c45185ed33005328dc14b4a244e40fc54b639a","608310dda6b5b6b9fc5da822455124bf9122715d46ade67d727200d7080bc45f",1},
    {"QUADN.SFD","edee5f2d216764864452741d752ee09f59b39e8e25e5933d2307cf0800f1072a","aa36d4ebc62d367f5e81db660b43cf72c0ff59b2170dca0e2522967f3a4f416e",1},
    {"QUADW.SFD","dc4988c994f37baabe34697df48cd2238e1648db87fea9476aa2c816d7cd59da","2f1dd843269fac7e9077104a538d799425ec4f771641c85c59250f1f32a109b7",1},
    {"SPYHUNN.SFD","7448b1e598404011923fb21d7b4826223dce0e5807cc914f8102b40a2f5da398","ceea6d4f09a6ad46542ab9fff88838e5380166181a55fc2d780f6594a907e80a",1},
    {"SPYHUNW.SFD","e728aad87b8f812ef19653888493a437b760ecd0f40e1902f132269a6f01cbde","12e6b17bf82918da4f22dd865cfd7ed575af5f8aa15b63f1d3a3fead8e038742",1},
    {"STRANGN.SFD","fff2a9feceb1618b14ba7cd8dd000952652d8b131e391cb9873f5a4714246707","aaff985238d4cecc97ddbf10c1f4c158a1c0e7928075d06a9f3d16c67b9bfc28",1},
    {"STRANGW.SFD","737c203deebda23483305f34740bc1916a594f7a4667b220832aa6b5cac95c76","a9906d042b42f6eae2643a1e77f565a8085fdda775d5a74f3d3bcabfd25b34ee",1},
    {"TITLEN.SFD","b184f25ad02e7ffc4ece847bc915c1f54e2eeddbf6afcd816be0108adadfa371","bbe9842b4fdb0763b9393467376d54ec83fc199e52b8251566a2a60d229c4147",2},
    {"TITLEW.SFD","9a850166ddc5a8a13d153498d329c42fe479d848b56ab2784c6f371c6b3b0928","3ba2d8aa47943c511560370b97eda2e5c9b8d6fae6ce7fe412b92ca79ccd5b2c",2},
};
static std::string utf8(const std::filesystem::path& p){auto b=p.generic_u8string();return {reinterpret_cast<const char*>(b.data()),b.size()};}
static void real(const std::filesystem::path& root){
 std::vector<std::string> actual,expected;
 for(const auto& e:std::filesystem::recursive_directory_iterator(root))if(e.is_regular_file()){
  auto ext=utf8(e.path().extension());std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return c>='a'&&c<='z'?char(c-'a'+'A'):char(c);});
  if(ext==".SFD")actual.push_back(utf8(std::filesystem::relative(e.path(),root)));
 }
 for(const auto& r:references)expected.push_back(r.key);
 std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());check(actual==expected,"all actual SFD paths match explicit38asset inventory");
 std::vector<MovieAssetCatalogEntry> catalog;for(const auto& r:references)catalog.push_back({r.key,r.key});MovieAssetFailure af;
 auto store=NativeMovieAssetStore::create(root,catalog,af);check(bool(store),"explicit verified asset catalog");
 unsigned headers=0,ordinary=0,events=0;std::vector<NativeMovieMetadata> retained;
 for(const auto& r:references){auto asset=store->open(r.key,af);check(bool(asset),"open actual catalog asset");
  NativeTestSha256 asset_hash;std::array<std::uint8_t,65536> buf{};
  for(;;){auto n=asset->read(buf.data(),buf.size(),af);check(!n.error,"asset source SHA read");if(!n.bytes)break;asset_hash.add(buf.data(),n.bytes);}
  check(asset_hash.finish()==r.asset_sha,"actual full source asset SHA");check(asset->seek(0,mka::content::SeekOrigin::begin,af),"rewind same owned asset");
  MovieContainerFailure cf;auto container=extract_movie_private2(*asset,cf);check(bool(container),"native container extraction");
  auto metadata=assemble(std::move(container->private2));check(metadata.sofdec_headers().size()==1&&metadata.origins().size()==r.ordinary,"actual header/ordinary classification counts");
  check(sha(metadata.ordinary_stream())==r.stream_sha&&sha(metadata.selected_root_bytes())==r.stream_sha,"actual whole stream and selected root SHA");
  MovieSubtitleFailure sf;auto bytes=metadata.selected_root_bytes();auto doc=NativeMovieSubtitles::parse(Bytes(bytes.begin(),bytes.end()),sf);check(bool(doc),"selected owned root reaches native subtitle parser");
  for(const auto& t:doc->tags())if(std::memcmp(t.raw_tag.data(),"TIMEDAT",7)==0)++events;
  headers+=unsigned(metadata.sofdec_headers().size());ordinary+=unsigned(metadata.origins().size());retained.push_back(std::move(metadata));
 }
 store.reset();check(retained.size()==38&&headers==38&&ordinary==40&&events==14,"all38/38header/40ordinary/14events source inventory");
 check(retained[0].ordinary_stream().size()>16,"owned bytes survive file/catalog lifetime");
 std::cout<<"actual assets=38 headers="<<headers<<" ordinary="<<ordinary<<" events="<<events<<'\n';
}
int main(int argc,char** argv){try{synthetic();if(argc==2)real(std::filesystem::u8path(argv[1]));else check(argc==1,"usage: test_movie_metadata [explicit-MOVIEPS2-root]");std::cout<<"PASS owned native metadata assembly; no player/callback execution\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
