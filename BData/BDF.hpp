#pragma once
#include <Collections/Array.hpp>
#include <Collections/String.hpp>
#include <Collections/StringMap.hpp>
#include <Math/vec2.h>
#include <Math/vec3.h>
#include <Math/vec4.h>


namespace BData::BDF
{

enum class ValueType : u8
{
    Null,
    Bool, // Boolean
    Int, // Signed 64 bits integer
    UInt, // Unsigned 64 bits integer
    Float, // 64 bits floating point number
    String, // Reflect plain text
    Vector2, // 2 dimension 32 bits vector
    Vector3, // 3 dimension 32 bits vector
    Vector4, // 4 dimension 32 bits vector
    Array, // Arbitrary sized collection of values
};

struct Value
{
    ValueType type = ValueType::Null;

    struct Array
    {
        usize begin;
        usize end;
    };

    union
    {
        bool boolean;
        i64 integer;
        u64 uinteger;
        f64 floating;
        Collections::StringView string;
        Vector2 vec2;
        Vector3 vec3;
        Vector4 vec4;
        Array array;
    };

    static constexpr Value Bool(bool boolean)
    {
        return Value{ValueType::Bool, {.boolean = boolean}};
    }

    static constexpr Value Integer(i64 integer)
    {
        return Value{ValueType::Bool, {.integer = integer}};
    }

    static constexpr Value UInteger(u64 uinteger)
    {
        return Value{ValueType::Bool, {.uinteger = uinteger}};
    }

    static constexpr Value Floating(f64 floating)
    {
        return Value{ValueType::Bool, {.floating = floating}};
    }
};

struct Segment
{
    Collections::String type;
    Collections::String name;

    // By name otherwise ""
    Collections::Array<Segment> segments;
    // By id
    Collections::StringMap<Value> values;

    // to create subviews
    Collections::Array<Value> arrays;

    Segment(Mem::Allocator& allocator, Collections::StringView type, Collections::StringView name)
    : type(allocator, 0, type), name(allocator, 0, name), segments(allocator, 4, {}), values(allocator, 4),
    arrays(allocator, 4, {})
    {}

    Value get_value_or(Collections::StringView name, Value fallback)
    {
        if(values.has(name))
        {
            return values.get(name);
        }

        return fallback;
    }

    Slice<Value> get_array(const Value::Array& array)
    {
        return arrays.slice().add(array.begin).slice(array.end - array.begin);
    }
};

struct Parser
{
    // Grammar:
    // comments   = ';' until new line
    // operator   = - ; This must to be at the left side of a number, spaces make it invalid
    // number     = [-] (int | uint | float) ;
    // bool       = true | false ;
    // scalar     = null | bool | number ;
    // value      = scalar | string ;
    // array      = '[' value [, value] ']' ;
    // segment    = identifier ["string"] '{' { segment | assignment } '}' ;
    // assignment = identifier '=' value ;
    // the following ones are one line: assignment
    static void parse(Mem::Allocator& allocator, Collections::StringView path, Collections::StringView content,
        Segment& segment, const IO::Writer& err);
};
    
struct Document
{
    DisableCopy(Document);
    DisableMove(Document);
    struct InternalData
    {
        Mem::Allocator& allocator;
        Slice<u8> content;
        
        Segment global_segment;

        InternalData(Mem::Allocator& allocator)
        : allocator(allocator), content(), global_segment(allocator, "--global--", "")
        {}
    } data;

    Document(Mem::Allocator& allocator, Collections::StringView path, const IO::Writer& err);
    ~Document();

    Segment& global_segment() { return data.global_segment; }
};

}
