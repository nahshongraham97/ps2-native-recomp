#pragma once
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

class NativeTestSha256 {
public:
    NativeTestSha256(){
        ULONG bytes=0,used=0;
        if(BCryptOpenAlgorithmProvider(&algorithm_,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 provider");
        if(BCryptGetProperty(algorithm_,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&bytes),sizeof(bytes),&used,0)<0){BCryptCloseAlgorithmProvider(algorithm_,0);algorithm_=nullptr;throw std::runtime_error("SHA256 object size");}
        try{object_.resize(bytes);}catch(...){BCryptCloseAlgorithmProvider(algorithm_,0);algorithm_=nullptr;throw;}
        if(BCryptCreateHash(algorithm_,&hash_,object_.data(),bytes,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm_,0);algorithm_=nullptr;throw std::runtime_error("SHA256 state");}
    }
    ~NativeTestSha256(){if(hash_)BCryptDestroyHash(hash_);if(algorithm_)BCryptCloseAlgorithmProvider(algorithm_,0);}
    NativeTestSha256(const NativeTestSha256&)=delete;
    NativeTestSha256& operator=(const NativeTestSha256&)=delete;
    void add(const void* p,std::size_t bytes){
        const auto* input=static_cast<const unsigned char*>(p);
        while(bytes){const auto n=static_cast<ULONG>(std::min<std::size_t>(bytes,std::numeric_limits<ULONG>::max()));
            if(BCryptHashData(hash_,const_cast<PUCHAR>(input),n,0)<0)throw std::runtime_error("SHA256 input");input+=n;bytes-=n;}
    }
    std::string finish(){
        std::array<unsigned char,32> digest{};
        if(BCryptFinishHash(hash_,digest.data(),digest.size(),0)<0)throw std::runtime_error("SHA256 finish");
        constexpr char h[]="0123456789abcdef";std::string result;
        for(const auto byte:digest){result.push_back(h[byte>>4]);result.push_back(h[byte&15]);}return result;
    }
private:
    BCRYPT_ALG_HANDLE algorithm_=nullptr;
    BCRYPT_HASH_HANDLE hash_=nullptr;
    std::vector<unsigned char> object_;
};
