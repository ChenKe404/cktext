/*
*******************************************************************************
    ChenKe404's text library
*******************************************************************************
@project	cktext
@authors	chenke404
@file	text.cpp
@brief 	chenke404's text source

// SPDX-License-Identifier: MIT
// Copyright (c) 2025 chenke404
******************************************************************************
*/


#include "text.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <filesystem>
#include <lz4xx.h>

namespace fs = std::filesystem;
constexpr auto L10KB = 10485760;

template<typename C>
inline size_t length(const C* str)
{ return std::char_traits<C>::length(str); }

//////////////////////////////////////////////////////////////////////////////////////////////////////
/// Text
//////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ck
{
using namespace ck::bio;

using u8str = Text::u8str;
using u32str =  Text::u32str;

template<typename Rd>
struct reader : bio::ireader
{
    inline reader(Rd* rd) :
        _rd(rd),
        _rdb(nullptr)
    {
    }

    using Rdb = bio::reader<bio::Buffer>;
    inline void attach(Rdb* rdb) {
        _rd = nullptr;
        _rdb = rdb;
    }

    inline size_t read(uint8_t* data, size_t capacity) override {
        if (_rdb)
            return _rdb->read(data, capacity);
        else if (_rd)
            return _rd->read(data, capacity);
        return 0;
    }

    inline size_t seek(pos_t pos) override {
        if (_rdb)
            return _rdb->seek(pos);
        else if (_rd)
            return _rd->seek(pos);
        return 0;
    }

    inline size_t pos() const override {
        if (_rdb)
            return _rdb->pos();
        else if (_rd)
            return _rd->pos();
        return 0;
    }

    inline size_t offset(pos_t ofs) override
    {
        if (_rd) return _rd->offset(ofs);
        else if (_rdb) return _rdb->offset(ofs);
        return 0;
    }

    template<typename T>
    inline size_t read(T* data, size_t size)
    {
        return read((uint8_t*)data, size);
    }
private:
    Rd* _rd = nullptr;
    Rdb* _rdb = nullptr;
};

using ctx_compress = lz4xx::context<lz4xx::Compress>;
struct writer
{
    inline writer(std::ofstream* so)
        : _so(so),_ctx(nullptr)
    {}
    inline void attach(ctx_compress* ctx) {
        _so = nullptr;
        _ctx = ctx;
    }

    inline bool write(const uint8_t *data, size_t size){
        if(_ctx)
            return _ctx->update(data,size);
        else if(_so)
        {
            const auto before = _so->tellp();
            _so->write((char*)data,size);
            return _so->tellp() - before == size;    // 真实写入大小不能小于输入数据大小
        }
        return false;
    }

    template<typename T>
    inline bool write(const T* data,size_t size)
    {
        return write((uint8_t*)data,size);
    }

private:
    std::ofstream* _so = nullptr;
    ctx_compress* _ctx = nullptr;
};

// 在析构时删除目标文件
struct temp_guard
{
    temp_guard() = default;
    inline temp_guard(const std::string& path)
        : _path(path)
    {}

    inline ~temp_guard()
    {
        if(!_path.empty())
        {
            std::error_code ec;
            fs::remove(_path,ec);
        }
    }

    inline void set(const std::string& path)
    { _path = path; }
private:
    std::string _path;
};

int read_str(ireader& rd, std::string& out)
{
    int sz = 0;
    rd.read((uint8_t*)&sz, 4);
    if (sz < 1 || sz > L10KB)    // 字符串长度不能超过10KB
        return 0;
    out.resize(sz);
    int pos = 0;
    int remain = sz;
    while (remain > 0)
    {
        if (remain > 1024)
            rd.read((uint8_t*)out.data() + pos, 1024);
        else
            rd.read((uint8_t*)out.data() + pos, remain);
        pos += 1024;
        remain -= 1024;
    }
    return sz;
}

bool read(ireader& rd, Text::Property& o)
{
    auto read_str = [&rd](std::string& out, int max_size) -> int {
        int sz = 0;
        rd.read((uint8_t*)&sz, 1);
        if (sz < 1 || sz > max_size)
            return -1;
        out.resize(sz);
        rd.read((uint8_t*)out.data(), sz);
        return sz;
    };

    // 读类型
    var::Type type = var::TP_NUL;
    rd.read((uint8_t*)&type, 1);

    // 是否在类型范围内, TP_NUL本就不该写入文件, 所以不考虑这个类型
    if (type < var::TP_BOOL || type > var::TP_STRING)
    {
        std::cerr << "Text::Property::read: illegal type was discovered! skiped." << std::endl;
        rd.offset(-1);
        return false;
    }

    // 读名称
    std::string name;
    auto sz_name = read_str(name, 64);
    if (sz_name < 0)
    {
        std::cerr << "Text::Attribute::read: illegal name length, it's must be (> 0 and <= 64)! skiped." << std::endl;
        rd.offset(-2);
        return false;
    }

    switch (type) {
    case var::TP_BOOL:
    {
        bool v = 0;
        rd.read((uint8_t*)&v, 1);
        o.set(name.c_str(), v);
    }
    break;
    case var::TP_INT:
    {
        int v = 0;
        rd.read((uint8_t*)&v, 4);
        o.set(name.c_str(), v);
    }
    break;
    case var::TP_FLOAT:
    {
        float v = 0;
        rd.read((uint8_t*)&v, 4);
        o.set(name.c_str(), v);
    }
    break;
    case var::TP_STRING:
    {
        std::string v;
        uint8_t sz_str = read_str(v, 255);     // 字符串值的长度不超过255字节
        if (sz_str < 0)
        {
            std::cerr << "Text::Attribute::read: illegal string length, it's must be (> 0 and <= 255)! skiped." << std::endl;
            rd.offset(-3 - sz_name);
            return false;
        }
        o.set(name.c_str(), v);
    }
    break;
    default: break;
    }
    return true;
}

void write(ck::writer& wt, const Text::Property& o)
{
    auto write_str = [&wt](const std::string& str) {
        auto sz = (int)str.size();
        wt.write(&sz, 1);
        wt.write(str.c_str(), sz);
    };

    for (auto& it : o)
    {
        const auto type = it.second.type();
        if (type == var::TP_NUL)
            continue;

        // 检查名称长度, 超长忽略
        const auto& name = it.first;
        if (name.empty() || name.size() > 64)
            continue;

        // 写类型
        wt.write(&type, 1);
        // 写名称
        write_str(name);
        // 写值
        switch (type) {
        case var::TP_BOOL:
        {
            bool v = it.second;
            wt.write((uint8_t*)&v, 1);
        }
        break;
        case var::TP_INT:
        {
            int v = it.second;
            wt.write((uint8_t*)&v, 4);
        }
        break;
        case var::TP_FLOAT:
        {
            float v = it.second;
            wt.write((uint8_t*)&v, 4);
        }
        break;
        case var::TP_STRING:
        {
            write_str(it.second);   // 字符串不为空 且 长度<=255, 这在Attribute::set中控制
        }
        break;
        default: break;
        }
    }
}

Text::Text()
{
    _map[""];
}

void CKT_CALL Text::u8to32(u8str in, std::u32string& out)
{
    auto sz_in = std::char_traits<char>::length(in);
    out.clear();
    out.reserve(sz_in);
    for (auto i = 0u;i < sz_in;)
    {
        uint8_t c = in[i];
        if (c < 0x7F)
        {
            out.push_back(c);
            ++i;
        }
        else if (c < 0xE0 && i < sz_in - 1)
        {
            out.push_back(((c ^ 0xC0) << 6) | (uint8_t(in[i + 1]) ^ 0x80));
            i += 2;
        }
        else if (c < 0xF0 && i < sz_in - 2)
        {
            out.push_back(((c ^ 0xE0) << 12) | ((uint8_t(in[i + 1]) ^ 0x80) << 6) | (uint8_t(in[i + 2]) ^ 0x80));
            i += 3;
        }
        else if (i < sz_in - 3)
        {
            out.push_back(((c ^ 0xF0) << 18) | ((uint8_t(in[i + 1]) ^ 0x80) << 12) | ((uint8_t(in[i + 2]) ^ 0x80) << 6) | (uint8_t(in[i + 3]) ^ 0x80));
            i += 4;
        }
        else
            break;
    }
    out.shrink_to_fit();
}

template<typename C>
void __u16to32(const C* in, std::u32string& out)
{
    auto sz_in = std::char_traits<C>::length(in);
    out.clear();
    out.reserve(sz_in);
    for (int i = 0;i < sz_in;++i)
    {
        char16_t c = in[i];
        if (c < 0xD800)
            out.push_back(c);
        else if (c <= 0xDBFF && i < sz_in - 1)
        {
            char32_t c32 = ((uint32_t)c ^ 0xD800) << 10 | in[i + 1] ^ 0xDC00;
            out.push_back(c32);
            ++i;
        }
    }
}

void CKT_CALL Text::u16to32(wstr in, std::u32string &out)
{
    __u16to32<wchar_t>(in, out);
}

void CKT_CALL Text::u16to32(u16str in, std::u32string &out)
{
    __u16to32<char16_t>(in, out);
}

template<class Rd>
static inline bool load(Text& that, Rd& _rd)
{
    ck::reader<Rd> rd(&_rd);
    // 读取文件标签和压缩标志
    bool compressed = true;
    char tag[3];
    rd.read(tag,3);
    rd.read(&compressed,1);
    if(strncmp(tag,"CKT",3) != 0)
    {
        std::cerr << "Text::open: illegal file tag! skiped." << std::endl;
        rd.offset(-3);
        return false;
    }

    buffer_t buf;
    lz4xx::reader_buffer rdb(&buf);
    if (compressed)
    {
        lz4xx::writer_buffer wtb(buf);
        lz4xx::decompress(rd, wtb);
        rd.attach(&rdb);
    }

    auto read_attr = [&rd](int sz,Text::Property& attr){
        attr.clear();
        for(int i=0; i<sz; ++i)
        {
            if(!read(rd,attr))  // 属性有误, 文件数据可能已被破坏
            {
                attr.clear();
                return false;
            }
        }
        return true;
    };

    // 读属性个数
    int sz_attr = 0;
    rd.read(&sz_attr,4);
    // 读组个数
    int sz_group = 0;
    rd.read(&sz_group,4);
    // 读属性
    if(!read_attr(sz_attr,that._prop))
        return false;

    bool error = false;
    std::string name;
    std::string src;
    Text::Group group;
    for(int i=0; i<sz_group; ++i)
    {
        // 读组名
        int sz_name = 0;
        rd.read(&sz_name,1);
        if(sz_name < 0 || sz_name > 64)   // 组名最长64字节, 名称可以为空, 因为默认组没有名称
        {
            std::cerr << "Text::open: illegal group name length was discovered! suspended." << std::endl;
            error = true;
            break;
        }
        if(sz_name == 0)
            name.clear();
        else
        {
            name.resize(sz_name);
            rd.read(name.data(),sz_name);
        }

        // 读属性个数
        int sz_attr = 0;
        rd.read(&sz_attr,4);
        // 读翻译个数
        int sz_item = 0;
        rd.read(&sz_item,4);
        // 读属性
        if(!read_attr(sz_attr,group._prop))
            return false;
        // 读取组的优先级
        auto var = group._prop.get("priority");
        if(var.type() == var::TP_INT)
            group._priority = (int)var;
        if(group._priority < 0)
            group._priority = 0;

        // 读取翻译项
        auto& map = group._map;
        for(int j=0; j<sz_item; ++j)
        {
            auto sz = read_str(rd,src);
            if(sz < 1)  // 原文必须有长度
            {
                error = true;
                break;
            }
            auto& trs = map[src];
            read_str(rd,trs);
        }

        auto& _map = that._map;
        auto iter = _map.find(name);
        if(iter == _map.end())  // 插入
            _map[name] = std::move(group);
        else    // 合并到已存在的组
        {
            for(auto& it : group)
            {
                iter->second.set(it.first.c_str(),it.second.c_str());
            }
        }

        group.clear();
    }
    if(error)
    {
        that.clear();
        return false;
    }

    return true;
}

bool Text::open(const char* filename)
{
    std::ifstream fi(filename, std::ios::binary);
    if (!fi) return false;
    auto rd = make_reader(fi);
    auto ret = ck::load(*this, rd);
    fi.close();
    update_sorted();
    return ret;
}

bool ck::Text::load(const uint8_t* buf, size_t size)
{
    auto rd = make_reader(buf, size);
    auto ret = ck::load(*this, rd);
    update_sorted();
    return ret;
}

bool Text::save(const char* filename, bool compress)
{
    std::ofstream fo(filename,std::ios::binary);
    if(!fo.is_open()) return false;
    ck::writer wt(&fo);

    // 写入"文件标签"和"压缩标志"
    wt.write("CKT",3);
    wt.write(&compress,1);

    // 写属性个数
    auto sz_attr = _prop.size();
    wt.write(&sz_attr,4);
    if(empty())
    {
        // 写组个数
        int sz_group = 0;
        wt.write(&sz_group,4);
        // 写属性
        write(wt,_prop);
        fo.close();
        return true;
    }
    else
    {
        // 写组个数
        auto sz_group = (int)_map.size();
        fo.write((char*)&sz_group,4);
        // 写属性
        write(wt,_prop);
    }

    size_t sz_name = 0;
    size_t sz_item = 0;
    for(auto& it : _map)
    {
        // 写组名
        sz_name = (int)it.first.size();
        wt.write(&sz_name,1);
        wt.write(it.first.c_str(),sz_name);

        auto& attr = it.second._prop;
        // 写属性个数
        sz_attr = attr.size();
        wt.write(&sz_attr,4);
        // 写翻译个数
        auto& map = it.second._map;
        sz_item = map.size();
        wt.write(&sz_item,4);
        // 写属性
        write(wt,attr);
        // 写翻译项
        for(auto& it : map)
        {
            // 原文
            auto sz = (int)it.first.size();
            fo.write((char*)&sz,4);
            fo.write(it.first.data(),sz);
            // 译文
            sz = (int)it.second.size();
            fo.write((char*)&sz,4);
            fo.write(it.second.data(),sz);
        }
    }

    fo.close();

    if(compress)
    {
        std::error_code ec;
        std::string path(filename);
        path.append(".cop");
        fo.open(path,std::ios::binary);
        if(!fo.is_open())
        {
            std::cerr << "Text::save: can't create cop file:" << path << std::endl;
            fs::remove(filename,ec);
            return false;
        }
        temp_guard tg(path);
        std::ifstream fi(filename, std::ios::binary);
        if(!fi.is_open())
        {
            std::cerr << "Text::save: can't open ckt file:" << filename << std::endl;
            return false;
        }
        // 不压缩"文件标签"和"压缩标志"
        fi.seekg(4);
        // 写入"文件标签"和"压缩标志"
        fo.write("CKT",3);
        fo.write((char*)&compress,1);

        lz4xx::progress pgs;
        lz4xx::reader_stream rd(fi);
        lz4xx::writer_stream wt(fo);
        lz4xx::preferences pfs;
        pfs.frame.blockSize = lz4xx::BS_Max1MB;
        if(!lz4xx::compress(rd,wt,&pgs,pfs))
        {
            std::cerr << "Text::open: failed to write ckt file:" << pgs.last_error << std::endl;
            return false;
        }
        fi.close();
        fo.close();
        fs::remove(filename,ec);
        fs::rename(path,filename,ec);
    }

    return true;
}

const char* g_empty = "";
u8str Text::u8(u8str src,u8str def) const
{
    if(!src) return nullptr;
    u8str trs = nullptr;
    for(auto& it : _sorted)
    {
        trs = it->u8(src,g_empty);
        if(trs)
        {
            if(trs == g_empty)  // 说明原文存在但译文不存在
                return def;
            else
                return trs;
        }
    }
    return nullptr;
}

u32str Text::u32(u8str src,u8str def) const
{
    if(!src) return nullptr;
    static std::u32string u32str;
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    u8str trs = nullptr;
    for(auto& it : _sorted)
    {
        trs = it->u8(src,g_empty);
        if(trs)
        {
            if(trs == g_empty)  // 说明原文存在但译文不存在
                u8to32(def, u32str);
            else
                u8to32(trs, u32str);
            return u32str.c_str();
        }
    }
    return nullptr;
}

bool Text::rename(const char *oldName, const char *newName)
{
    auto it = _map.extract(oldName);
    if(!it) return false;
    it.key() = newName;
    bool flag = true;
    if(!_map.insert(std::move(it)).inserted)
    {
        flag = false;
        // 重命名失败, 还原
        it.key() = oldName;
        _map.insert(std::move(it));
    }
    return flag;
}

Text::Property &Text::prop()
{
    return _prop;
}

const Text::Property &Text::prop() const
{
    return _prop;
}

Text::Group *Text::get(const char *group)
{
    if(!group) group = "";
    auto iter = _map.find(group);
    if(iter == _map.end())
        return nullptr;
    return &iter->second;
}

const Text::Group *Text::get(const char *group) const
{
    if(!group) group = "";
    auto iter = _map.find(group);
    if(iter == _map.end())
        return nullptr;
    return &iter->second;
}

bool Text::empty() const
{
    // 只有默认组且默认组是空的
    if(_map.size() < 2)
        return _map.begin()->second.empty();
    return false;
}

void Text::clear()
{
    _prop.clear();
    // 始终保留默认组
    for(auto i=_map.begin(); _map.size() > 1;)
    {
        if(i->first.empty())
        {
            ++i;
            continue;
        }
        i = _map.erase(i);
    }
    _map.begin()->second.clear();
    update_sorted();
}

void Text::remove(const char *group)
{
    if(!group) return;
    const auto len = length(group);
    if(len > 0)
        _map.erase(group);
    else    // 如果是移除默认组, 则只是删除默认组的项, 保留组本身
    {
        auto iter = _map.find(group);
        if(iter != _map.end())
            iter->second.clear();
    }
    update_sorted();
}

Text::iterator Text::remove(iterator it)
{
    auto ret = _map.erase(it);
    update_sorted();
    return ret;
}

Text::Group* Text::insert(const char *group, const Property& prop)
{
    if(!group) return nullptr;
    const auto len = length(group);
    if(len < 1 || len > 64)
    {
        std::cerr << "Text::insert: illegal group name length, it's must be (> 0 and <= 64)!" << std::endl;
        return nullptr;
    }
    auto ret = _map.insert({ group,{} });
    if(!ret.second)
    {
        std::cerr << "Text::insert: can't inserted. maybe already have same name group!" << std::endl;
        return nullptr;
    }
    auto& grp = ret.first->second;
    grp._prop = prop;
    auto var = prop.get("priority");
    if(var.type() == var::TP_INT)
        grp._priority = (int)var;
    if(grp._priority < 0)
        grp._priority = 0;
    update_sorted();
    return &grp;
}

Text::iterator Text::begin() const
{
    return _map.begin();
}

Text::iterator Text::end() const
{
    return _map.end();
}

void Text::update_sorted()
{
    _sorted.clear();
    for(auto& it : _map){
        _sorted.push_back(&it.second);
    }
    std::sort(_sorted.begin(),_sorted.end(),[](Group* a,Group* b){
        return a->_priority > b->_priority || (a->_priority == b->_priority && a < b);
    });
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/// Text::Group
//////////////////////////////////////////////////////////////////////////////////////////////////////
Text::u8str Text::Group::u8(u8str src,u8str def) const
{
    auto iter = _map.find(src);
    if(iter == _map.end())
        return nullptr;
    const auto& trs = iter->second;
    if(trs.empty()) // 译文为空则返回def
        return def;
    return trs.c_str();
}

Text::u32str Text::Group::u32(u8str src,u8str def) const
{
    static std::u32string u32str;
    static std::mutex mtx;
    std::lock_guard<std::mutex> lock(mtx);

    auto iter = _map.find(src);
    if(iter == _map.end())
        return nullptr;
    const auto& trs = iter->second;
    if(trs.empty()) // 译文为空则返回def
        u8to32(def, u32str);
    else
        u8to32(trs.c_str(), u32str);
    return u32str.c_str();
}

Text::Property &Text::Group::prop()
{
    return _prop;
}

const Text::Property &Text::Group::prop() const
{
    return _prop;
}

bool Text::Group::empty() const
{
    return _map.empty();
}

void Text::Group::clear()
{
    _prop.clear();
    _map.clear();
    _priority = 100;
}

void Text::Group::remove(u8str src)
{
    _map.erase(src);
}

Text::Group::iterator Text::Group::remove(iterator it)
{
    return _map.erase(it);
}

Text::u8str Text::Group::set(u8str src, u8str trs)
{
    if(!src || !trs)
        return nullptr;
    const auto sz_src = length(src);
    const auto sz_trs = length(trs);
    if(sz_src < 1 || sz_src > L10KB || sz_trs > L10KB)
        return nullptr;
    auto& it = _map[src];
    it = trs;
    return it.c_str();
}

Text::Group::iterator Text::Group::begin() const
{
    return _map.begin();
}

Text::Group::iterator Text::Group::end() const
{
    return _map.end();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/// Text::Attribute
//////////////////////////////////////////////////////////////////////////////////////////////////////
var Text::Property::get(const char *name) const
{
    if(!name) return {};
    auto iter = _map.find(name);
    if(iter == _map.end())
        return {};
    return iter->second;
}

void Text::Property::set(const char *name, const var &value)
{
    if(!name || !value.valid())
        return;
    auto len = length(name);
    if(len < 1 || len > 64)
    {
        std::cerr << "Text::Property::set: illegal name length, it's must be (> 0 and <= 64)!" << std::endl;
        return;
    }
    if(value.type() == var::TP_STRING)
    {
        const std::string& str = value;
        len = (int)str.size();
        if(len < 1 || len > 255)
        {
            std::cerr << "Text::Attribute::set: illegal string length, it's must be (> 0 and <= 255)!" << std::endl;
            return;
        }
    }
    _map[name] = value;
}

size_t Text::Property::size() const
{
    return _map.size();
}

bool Text::Property::empty() const
{
    return _map.empty();
}

void Text::Property::clear()
{
    _map.clear();
}

void Text::Property::remove(const char *name)
{
    _map.erase(name);
}

Text::Property::iterator Text::Property::remove(iterator it)
{
    return _map.erase(it);
}

Text::Property::iterator Text::Property::begin() const
{
    return _map.begin();
}

Text::Property::iterator Text::Property::end() const
{
    return _map.end();
}

}
