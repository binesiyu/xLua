//
//  luaProfile.hpp
//  KingdomofHeaven
//
//  Created by Chen Chen on 2019/4/29.
//

#ifndef luaProfile_hpp
#define luaProfile_hpp

#include <stdio.h>
#include <string>
#include <map>
#include <chrono>
#include <memory>

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
    #include "lualib.h"
    
    int luaopen_luaProfiler(lua_State *L);
}
using namespace std;

class FunctionCallInfo {
public:
    int numbers;
    chrono::steady_clock::time_point  startTime;
    double time;
};

class LineCallInfo{
public:
    int line;
    string strSource;
    string name;
    int mallocSize;
    int callNums;
};

struct lmp_block {
    size_t size;
    size_t luatype;
    void* ptr;
    string strSource;
};

class LuaProfiler
{
public:
    void SetLogPath(string path);
    
    void Start(lua_State *L);
    
    void WriteLog();
    
    void WriteCSV();
    
    void Stop();
    
    void clear();
    
    static LuaProfiler &getInstance() {
        static LuaProfiler luaP;
        return luaP;
    }
    void CallFunc(lua_State *L,lua_Debug *ar);
    void CallLine(lua_State *L,lua_Debug *ar);
    void Malloc(void *ptr, size_t nsize, size_t luatype);
    void Free(void *ptr);
    void Realloc(void *ptr, size_t nsize, void *p);
private:
    std::string logPath;
    map<string, std::shared_ptr<FunctionCallInfo>> functionCalls;
    map<intptr_t, std::shared_ptr<lmp_block>> memoryBlocks;
    map<size_t, std::shared_ptr<lmp_block>> totalMemory;
    map<size_t, std::shared_ptr<lmp_block>> freeSize;
    map<string, std::shared_ptr<LineCallInfo>> lineMallocs;
    map<string, std::shared_ptr<LineCallInfo>> lineFrees;
    
    uintptr_t lowAddress;
    uintptr_t maxAddress;
    
    void PrepareType(size_t luatype);
    
    LineCallInfo lineInfo;
};



#endif /* luaProfile_hpp */
