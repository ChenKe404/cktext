/*
*******************************************************************************
    ChenKe404's text library
*******************************************************************************
@project	cktext
@authors	chenke404
@file	var.hpp
@brief 	a variant class to accept base value type

// SPDX-License-Identifier: MIT
// Copyright (c) 2025 chenke404
******************************************************************************
*/


#ifndef CK_VAR_HPP
#define CK_VAR_HPP

#include <string>
#include <variant>

namespace ck
{

struct var
{
#define DEF_TYPE(Typ,Def) \
    inline var(const Typ& v) : d(v){} \
    inline operator const Typ&() const { return get<Typ>(Def); } \
    inline var& operator=(Typ v) { d=v; return *this; }

    enum Type
    {
        TP_NUL,
        TP_BOOL,
        TP_INT,
        TP_FLOAT,
        TP_STRING
    };

    inline var() = default;
    inline var(const char* v) : d(v){}

    DEF_TYPE(bool,false)
    DEF_TYPE(int,0)
    DEF_TYPE(float,0)
    DEF_TYPE(std::string,"")

    inline Type type() const
    {
        switch (d.index()) {
        case 0: return TP_BOOL;
        case 1: return TP_INT;
        case 2: return TP_FLOAT;
        case 3: return TP_STRING;
        }
        return TP_NUL;
    }

    inline bool valid() const { return !d.valueless_by_exception(); }
private:
    template<typename T>
    inline const T& get(const T& def) const {
        if(auto v = std::get_if<T>(&d))
            return *v;
        return def;
    }
private:
    std::variant<
        bool,
        int,
        float,
        std::string
        > d;

#undef DEF_TYPE
};

}

#endif // CK_VAR_HPP
