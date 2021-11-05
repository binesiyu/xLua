//
//  luaProfile.cpp
//  KingdomofHeaven
//
//  Created by Chen Chen on 2019/4/29.
//

#include "luaProfile.hpp"
#include <cstdlib>
#include "cocos2d.h"
#include <fstream>
#include <iostream>
using namespace cocos2d;
extern "C" {
    
    static void callhook(lua_State *L, lua_Debug *ar) {

        if(ar->event == LUA_HOOKLINE)
            LuaProfiler::getInstance().CallLine(L, ar);
        else
            LuaProfiler::getInstance().CallFunc(L, ar);
        
    }
    
    static void callLine(lua_State *L, lua_Debug *ar) {
//        lua_Debug previous_ar;
//        lua_getstack(L, 1, &previous_ar);
//        lua_getinfo(L, "n", ar);
        LuaProfiler::getInstance().CallLine(L, ar);
    }
    
    static void *lmp_free(void *ptr) {
        LuaProfiler::getInstance().Free(ptr);
        free(ptr);
        return NULL;
    }
    static void *lmp_malloc(size_t nsize, size_t luatype) {
        void *ptr = malloc(nsize);
        LuaProfiler::getInstance().Malloc(ptr, nsize, luatype);
        return ptr;
    }
    static void *lmp_realloc(void *ptr, size_t nsize) {
        void *p = realloc(ptr, nsize);
        LuaProfiler::getInstance().Realloc(ptr, nsize, p);
        return p;
    }
    static void *lmp_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
        if(nsize == 0) {
            return lmp_free(ptr);
        }else if(ptr == NULL) {
            return lmp_malloc(nsize, osize);
        }else {
            return lmp_realloc(ptr, nsize);
        }
    }
}


void LuaProfiler::Realloc(void *ptr, size_t nsize, void *p) {
    if(p == NULL) {
    }else {
        auto find = memoryBlocks.find((intptr_t)ptr);
        if(find != memoryBlocks.end()) {
            auto sec = find->second;
            memoryBlocks.erase(find);
            memoryBlocks[(intptr_t)p] = sec;
            auto oldSize = sec->size;
            sec->size = nsize;
            
            auto lt = sec->luatype;
            PrepareType(lt);
            totalMemory[lt]->size -= oldSize;
            totalMemory[lt]->size += nsize;
            
            
        }
        
    }
}

void LuaProfiler::Malloc(void *ptr, size_t nsize, size_t luatype) {
    //LOG(INFO) << "TP:" << luatype << ":" << nsize;
    auto lmpBlock = shared_ptr<lmp_block>(new lmp_block());
    lmpBlock->size = nsize;
    lmpBlock->luatype = luatype;
    memoryBlocks[(intptr_t)ptr] = lmpBlock;
    
    PrepareType(luatype);
    totalMemory[luatype]->size += nsize;
    
    if((uintptr_t)ptr > maxAddress) {
        maxAddress = (uintptr_t)ptr;
    }
    
    if(lineInfo.strSource.length() == 0)
        return;
    
    char szTmp[256] = {0,};
    if(lineInfo.strSource.length() > 200)
        lineInfo.strSource = lineInfo.strSource.substr(lineInfo.strSource.length()-200,lineInfo.strSource.length());
    
    sprintf(szTmp, "%s_%d", lineInfo.strSource.c_str(), lineInfo.line);
    string key = szTmp;
    lmpBlock->strSource = szTmp;
    
    auto ret = lineMallocs.find(key);
    if(ret != lineMallocs.end()){
        ret->second->mallocSize += nsize;
        ret->second->callNums++;
    }
    else{
        shared_ptr<LineCallInfo> lc(new LineCallInfo());
        lineMallocs[key] = lc;
        lc->mallocSize = nsize;
        lc->strSource = lineInfo.strSource;
        lc->name = lineInfo.name;
        lc->line = lineInfo.line;
        lc->callNums = 1;
    }
    
}

void LuaProfiler::Free(void *ptr) {
    
    auto find = memoryBlocks.find((intptr_t)ptr);
    if(find != memoryBlocks.end()) {
        auto lt = find->second->luatype;
        
        auto sz = find->second->size;
        
        PrepareType(lt);
        totalMemory[lt]->size -= sz;
        freeSize[lt]->size += sz;
        
//        memoryBlocks.erase(find);
        
        if(find->second->strSource.length() == 0)
        {
            memoryBlocks.erase(find);
            return;
        }
        
//        char szTmp[256] = {0,};
//        if(lineInfo.strSource.length() > 200)
//            lineInfo.strSource = lineInfo.strSource.substr(lineInfo.strSource.length()-200,lineInfo.strSource.length());
//
//        sprintf(szTmp, "%s_%d", lineInfo.strSource.c_str(), lineInfo.line);
//        string key = szTmp;
        string key = find->second->strSource;
        auto ret = lineFrees.find(key);
        if(ret != lineFrees.end()){
            ret->second->mallocSize += sz;
            ret->second->callNums++;
        }
        else{
            shared_ptr<LineCallInfo> lc(new LineCallInfo());
            lineFrees[key] = lc;
            lc->mallocSize = sz;
            lc->strSource = key;
            lc->name = lineInfo.name;
            lc->line = lineInfo.line;
            lc->callNums = 1;
        }
        memoryBlocks.erase(find);
    }
}

void LuaProfiler::SetLogPath(string path) {
    logPath = path;
//    [DDLog addLogger:[DDTTYLogger sharedInstance]];
//    DDFileLogger *fileLogger = [[DDFileLogger alloc] init];
//    fileLogger.rollingFrequency = 60*60*24;
//    fileLogger.logFileManager.maximumNumberOfLogFiles = 7;
    //fileLogger.
//    [DDLog addLogger:fileLogger];
    
    /*
     el::Configurations defaultConf;
     defaultConf.setToDefault();
     defaultConf.set(el::Level::Info, el::ConfigurationType::Enabled, "true");
     defaultConf.set(el::Level::Info, el::ConfigurationType::Format, "%datetime %msg");
     //defaultConf.set(el::Level::Info, el::ConfigurationType::ToFile, "true");
     defaultConf.set(el::Level::Info, el::ConfigurationType::Filename, logPath);
     //defaultConf.set(el::Level::Info, el::ConfigurationType::ToStandardOutput, "false");
     
     el::Loggers::reconfigureLogger("default", defaultConf);
     */
}

void LuaProfiler::PrepareType(size_t luatype) {
    if(totalMemory.find(luatype) == totalMemory.end()) {
        auto lp = totalMemory[luatype] = shared_ptr<lmp_block>(new lmp_block());
        lp->luatype = luatype;
        lp->size = 0;
    }
    if(freeSize.find(luatype) == freeSize.end()) {
        auto lp = freeSize[luatype] = shared_ptr<lmp_block>(new lmp_block());
        lp->luatype = luatype;
        lp->size = 0;
    }
}
void LuaProfiler::Start(lua_State *L){
    lowAddress = (uintptr_t)L;
    maxAddress = lowAddress;
    
    
    
    lua_sethook(L, (lua_Hook)callhook, LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE , 0);
//    lua_sethook(L, (lua_Hook)callLine, LUA_MASKLINE, 0);
    lua_setallocf(L, lmp_alloc, NULL);
}

void LuaProfiler::CallFunc(lua_State *L, lua_Debug *ar) {
    lua_getinfo(L,"n", ar);
    if(ar->name == NULL) {
        return;
    }
    
    auto ret = functionCalls.find(ar->name);
    if(ret == functionCalls.end()){
        if(ar->event == 1) {
            return;
        }
        
        shared_ptr<FunctionCallInfo> fc(new FunctionCallInfo());
        functionCalls[ar->name] = fc;
        fc->numbers = 0;
        fc->time = 0;
    }
    
    shared_ptr<FunctionCallInfo> fc1 = functionCalls[ar->name];
    
    if(!ar->event) {
        fc1->startTime = chrono::steady_clock::now();
    }else {
        auto diff = chrono::steady_clock::now()-fc1->startTime;
        auto d = chrono::duration<double, milli> (diff).count();
        fc1->time += d;
        fc1->numbers++;
    }
}

void LuaProfiler::CallLine(lua_State *L, lua_Debug *ar){
//    lua_Debug previous_ar;
//    uint32_t level = 0;

//    lua_getstack(L, level, &previous_ar);
//    lua_getinfo(L, "nlS", &previous_ar);
    lua_getinfo(L, "<nlS", ar);
    lineInfo.line = ar->currentline;
    lineInfo.strSource = ar->source ? ar->source : "";
    lineInfo.name = ar->name ? ar->name: "";
    
}

void LuaProfiler::WriteLog() {
    FILE *fp;
    string targetPath = logPath + "memlog.txt";
    fp= fopen(targetPath.c_str(), "w+");
    if(!fp)
        return;
    
    fseek(fp,0,SEEK_END);
    fprintf(fp, "BEGIN:\n");
    
    for(auto f : functionCalls){
        auto fn = f.first;
        auto fc = f.second;
        //LOG(INFO) << "FC:" << fn << ":" << fc->time << ":" << fc->numbers;
        fprintf(fp,"FC:%s:%lf:%d \n", fn.c_str(), fc->time, fc->numbers);
    }
    
//    functionCalls.clear();
    
    for(auto l : lineMallocs){
//        auto key = l.first;
        if(l.second->mallocSize > 2000)
            fprintf(fp,"LMalloc:%s line:%d, malloc:%d call:%d \n",l.second->strSource.c_str(), l.second->line, l.second->mallocSize, l.second->callNums);
    }


//    for(auto l : lineFrees){
//        //        auto key = l.first;
//        if(l.second->mallocSize > 2000)
//        fprintf(fp, "LFree:%s line:%d, free:%d call:%d \n",l.second->strSource.c_str(), l.second->line, l.second->mallocSize, l.second->callNums);
//    }
    
   for(auto& l : lineMallocs){
       auto f = lineFrees.find(l.first);
       int leak = l.second->mallocSize;
//       int mallocNum = l.second->mallocSize;
//       int freeNum = 0;
       if(f != lineFrees.end())
       {
           leak = l.second->mallocSize - f->second->mallocSize;
       }
       if(leak > 1000)
           fprintf(fp,"LCache:%s line:%d, leak:%d \n",l.second->strSource.c_str(), l.second->line, leak);
    }
    
//    for(auto& m: memoryBlocks){
//         fprintf(fp,"LLeak:%s line:%d, luaType:%d \n",m.second->strSource.c_str(), m.second->size, m.second->luatype);
//    }
    
    lineMallocs.clear();
    lineFrees.clear();
    
    
    size_t totalMem = 0;
    for(auto m : totalMemory) {
        auto t = m.first;
        auto sz = m.second->size;
        //LOG(INFO) << "MEM:" << t << ":" << sz;
        fprintf(fp,"MEM:%lu:%lu \n", t, sz);
        totalMem += sz;
    }
    
    for(auto m : freeSize) {
        auto t = m.first;
        auto sz = m.second->size;
        //LOG(INFO) << "FREE:" << t << ":" << sz;
        fprintf(fp,"FREE:%lu:%lu \n", t, sz);
        m.second->size = 0;
    }
    
    
    //LOG(INFO) << "TOTALMEM:" << totalMem;
    fprintf(fp,"TOTALMEM:%lu \n", totalMem);
    //totalMemory.clear();
    //memoryBlocks.clear();
    fprintf(fp, "END: \n");
    fclose(fp);
}

void LuaProfiler::WriteCSV()
{
    ofstream outFile;
    string targetPath = logPath + "memFuncCall.csv";
    outFile.open(targetPath.c_str(), ios::out);
    outFile << "funcName" << "," << "time" << "," << "callNums" << endl;
    for(auto f : functionCalls){
        auto fn = f.first;
        auto fc = f.second;
        //LOG(INFO) << "FC:" << fn << ":" << fc->time << ":" << fc->numbers;
        outFile << fn.c_str() << "," << fc->time << "," << fc->numbers << endl;
    }
    outFile << endl;
    outFile.close();
    
    targetPath = logPath + "memMalloc.csv";
    outFile.open(targetPath.c_str(), ios::out);
    outFile << "lineMallocInfo" << endl;
    outFile << "source" << "," << "line" << "," << "malloc" << "," << "callNums" << endl;
    for(auto l : lineMallocs){
        //        auto key = l.first;
//        if(l.second->mallocSize > 2000)
            outFile << l.second->strSource.c_str() << "," << l.second->line << "," << l.second->mallocSize << "," << l.second->callNums << endl;
    }
    outFile.close();
    
    targetPath = logPath + "memLeak.csv";
    outFile.open(targetPath.c_str(), ios::out);
    outFile << "mallocSource" << "," << "line" << "," << "leak"  << endl;
    for(auto& l : lineMallocs){
        auto f = lineFrees.find(l.first);
        int leak = l.second->mallocSize;
        //       int mallocNum = l.second->mallocSize;
        //       int freeNum = 0;
        if(f != lineFrees.end())
        {
            leak = l.second->mallocSize - f->second->mallocSize;
        }
//        if(leak > 1000)
            outFile << l.second->strSource.c_str() << "," << l.second->line << "," << leak  << endl;
    }
    
    size_t totalMem = 0;
    
    for(auto m : totalMemory) {
        auto t = m.first;
        auto sz = m.second->size;
        outFile << "MEM" << "," << t << "," << sz << endl;
        totalMem += sz;
    }
    
    for(auto m : freeSize) {
        auto t = m.first;
        auto sz = m.second->size;
        //LOG(INFO) << "FREE:" << t << ":" << sz;
        outFile << "FREE" << "," << t << "," << sz << endl;
        m.second->size = 0;
    }
    
    
    //LOG(INFO) << "TOTALMEM:" << totalMem;
    outFile << "TOTALMEM" << "," << totalMem << endl;
    outFile.close();
}

void LuaProfiler::Stop() {
    //el::Loggers::flushAll();
//    [DDLog flushLog];
    clear();
}

void LuaProfiler::clear() {
    functionCalls.clear();
    lineMallocs.clear();
    lineFrees.clear();
}

extern "C" {
    
    static int profiler_init(lua_State *L){
        LuaProfiler::getInstance().Start(L);
        return 0;
    }
    static int profiler_stop(lua_State *L) {
        LuaProfiler::getInstance().Stop();
        return 0;
    }
    
    static int profiler_setLogPath(lua_State *L) {
        const char*path = lua_tostring(L, 1);
        LuaProfiler::getInstance().SetLogPath(path);
        return 0;
    }
    static int profiler_writeLog(lua_State *L) {
        LuaProfiler::getInstance().WriteLog();
        return 0;
    }
    
    static int profiler_writeCSV(lua_State* L) {
        LuaProfiler::getInstance().WriteCSV();
        return 0;
    }
    
    static int profiler_clear(lua_State* L) {
        LuaProfiler::getInstance().clear();
        return 0;
    }
    
    static const luaL_Reg prof_funcs[] = {
        {"start", profiler_init},
        {"stop", profiler_stop},
        {"setLogPath", profiler_setLogPath},
        {"writeLog", profiler_writeLog},
        {"writeCSV", profiler_writeCSV},
        {"clear", profiler_clear},
        {NULL, NULL},
    };
    
    int luaopen_luaProfiler(lua_State *L) {
        luaL_openlib(L, "luaProfiler", prof_funcs, 0);
        return 1;
    }
}
