#pragma once
#include <collections/array.h>
#include <collections/string_map.h>
#include <math/vec2.h>
#include <math/vec3.h>
#include <math/vec4.h>


namespace BData::BDF
{

enum class ValueType : u8
{
    Null,
    Bool,
    Int,
    UInt,
    Float,
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
        StringView string;
    };
};

struct Segment
{
    String type;
    String name;

    StringMap<Segment> segments;
    StringMap<Value> values;

    Segment(Mem::Allocator& allocator, StringView type, StringView name)
    : type(allocator, 0, type), name(allocator, 0, name), segments(allocator, 4), values(allocator, 4)
    {}
};

struct Parser
{
    // Grammar:
    // comments = ; until new line
    // segment    = identifier [identifier] '{' { segment | assignment } '}' ;
    // assignment = identifier '=' value ;
    // value      = null | bool | number | string | '(' number { ',' number } ')' ;
    static void parse(Mem::Allocator& allocator, StringView content, Segment& segment);
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

    Document(Mem::Allocator& allocator, StringView path);
    ~Document();
};

}
