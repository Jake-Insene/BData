#pragma once
#include <Collections/Array.hpp>
#include <Collections/String.hpp>
#include <Collections/StringMap.hpp>
#include <math/vec2.h>
#include <math/vec3.h>
#include <math/vec4.h>


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
};

struct Value
{
    ValueType type = ValueType::Null;

    union
    {
        bool boolean;
        i64 integer;
        u64 uinteger;
        f64 floating;
        Collections::StringView string;
    };
};

struct Segment
{
    Collections::String type;
    Collections::String name;

    // By name otherwise ""
    Collections::StringMap<Segment> segments;
    // By id
    Collections::StringMap<Value> values;

    Segment(Mem::Allocator& allocator, Collections::StringView type, Collections::StringView name)
    : type(allocator, 0, type), name(allocator, 0, name), segments(allocator, 4), values(allocator, 4)
    {}
};

struct Parser
{
    // Grammar:
    // comments   = ';' until new line
    // number     = int | uint | float;
    // scalar     = null | bool | number;
    // value      = scalar | string;
    // segment    = identifier ["string"] '{' { segment | assignment } '}' ;
    // assignment = identifier '=' value ;
    static void parse(Mem::Allocator& allocator, Collections::StringView content, Segment& segment);
};
    
struct Document
{
    DisableCopy(Document);
    DisableMove(Document);
    struct InternalData
    {
        Segment global_segment;

        InternalData(Mem::Allocator& allocator)
        : global_segment(allocator, "--global--", "")
        {}
    } data;

    Mem::Allocator& allocator;
    Slice<u8> content;

    Document(Mem::Allocator& allocator, Collections::StringView path);
    ~Document();

    Segment& global_segment() { return data.global_segment; }
};

}
