/*
*******************************************************************************
    ChenKe404's text library
*******************************************************************************
@project	cktext
@authors	chenke404
@file	text.h
@brief 	chenke404's text header

// SPDX-License-Identifier: MIT
// Copyright (c) 2025 chenke404
******************************************************************************
*/


#ifndef CK_TEXT_H
#define CK_TEXT_H

#ifdef _WIN32
#define CKT_CALL __stdcall
#else
#define CKT_CALL
#endif

/*
 * 文本翻译管理类, 原文和译文均使用utf-8编码
 */

#include <map>
#include <string>
#include <var.hpp>

namespace ck
{

struct Text
{
    using u8str = const char*;
    using wstr = const wchar_t*;
    using u16str = const char16_t*;
    using u32str = const char32_t*;

    // 自定义属性
    struct Property
    {
        using container = std::map<std::string,var>;
        using iterator = container::const_iterator;

        var get(const char* name) const;
        void set(const char* name, const var& value);
        size_t size() const;

        bool empty() const;
        void clear();
        void remove(const char* name);

        iterator begin() const;
        iterator end() const;
        iterator remove(iterator);
    private:
        container _map;
    };

    struct Group
    {
        using container = std::map<std::string,std::string>;
        using iterator = container::const_iterator;

        // 获取译文
        // @def 译文不存在时的返回值
        // @return utf8译文 或 nullptr(原文不存在) 或 def(译文不存在)
        u8str u8(u8str src,u8str def = nullptr) const;

        // 获取译文
        // @def 译文不存在时的返回值
        // @return utf32译文 或 nullptr(原文不存在) 或 def(译文不存在)
        u32str u32(u8str src,u8str def = nullptr) const;

        Property& prop();
        const Property& prop() const;

        bool empty() const;
        void clear();
        void remove(u8str src);
        // 插入翻译
        // @src utf8编码原文
        // @trs utf8编码译文
        // @return 插入成功返回插入的译文, 否则返回nullptr
        u8str set(u8str src, u8str trs);

        iterator begin() const;
        iterator end() const;
        iterator remove(iterator);
    private:
        Property _prop;
        container _map;
        friend struct Text;
    };

    using container = std::map<std::string,Group>;
    using iterator = container::const_iterator;

    Text();
    Text(const Text&) = delete;

    // UTF8 to UTF32-BE
    static void CKT_CALL u8to32(u8str,std::u32string& out);
    // UTF16-BE to UTF32-BE
    static void CKT_CALL u16to32(wstr, std::u32string& out);
    // UTF16-BE to UTF32-BE
    static void CKT_CALL u16to32(u16str, std::u32string& out);

    bool open(const char*);
    bool save(const char*, bool compress = false);

    // 获取第一个匹配的译文
    // @def 译文不存在时的返回值
    // @return utf8译文 或 nullptr(原文不存在) 或 def(译文不存在)
    u8str u8(u8str src,u8str def = nullptr) const;

    // 获取第一个匹配的译文
    // @def 译文不存在时的返回值
    // @return utf8译文 或 nullptr(原文不存在) 或 def(译文不存在)
    u32str u32(u8str src,u8str def = nullptr) const;

    Property& prop();
    const Property& prop() const;

    // 获取组
    // @group 组名, 为nullptr则返回无名的默认组
    Group* get(const char* group = nullptr);
    const Group* get(const char* group = nullptr) const;

    // 重命名组
    bool rename(const char* oldName,const char* newName);
    bool empty() const;
    void clear();
    void remove(const char* group);
    // 插入组
    // @return 返回插入组的指针, 失败返回nullptr
    Group* insert(const char* group, const Property& prop = {});

    iterator begin() const;
    iterator end() const;
    iterator remove(iterator);
private:
    Property _prop;
    container _map;
};

}

#endif // !CK_TEXT_H
