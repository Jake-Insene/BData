#include "BData/BDF.hpp"

#include <io/file.h>


namespace BData::BDF
{

static bool is_digit(const char c)
{
    return c >= '0' && c <= '9';
}

static bool is_identifier_start(const char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_identifier_character(const char c)
{
    return is_identifier_start(c) || is_digit(c);
}

struct Reader
{
    Mem::Allocator& allocator;
    StringView content;
    usize position;

    Reader(Mem::Allocator& allocator, StringView content)
    : allocator(allocator), content(content), position(0)
    {}

    char current() const { return position < content.len ? content[position] : '\0'; }
    void advance() { position++; }

    bool same_as_and_advance(StringView id)
    {
        for(usize i = 0; i < id.len; i++)
        {
            if(position+i >= content.len)
            {
                return false;
            }

            if(content[position + i] != id[i])
            {
                return false;
            }
        }

        for(usize i = 0; i < id.len; i++)
        {
            advance();
        }

        return true;
    }

    void advance_until_new_line()
    {
        while(current() != '\n' && current() != '\0')
        {
            advance();
        }
    }

    void skip_whitespace_and_comments()
    {
        while(current() == ' '
            || current() == '\t'
            || current() == '\r'
            || current() == '\n'
            || current() == ';')
        {
            if(current() == ';')
            {
                advance();
                advance_until_new_line();
            }
            else
            {
                advance();
            }
        }
    }

    StringView read_identifier()
    {
        usize begin = position;

        while(is_identifier_character(current()))
        {
            advance();
        }

        return content.add(begin).slice(position - begin);
    }

    Value read_number()
    {
        bool is_real = false;

        usize begin = position;
        while(is_digit(current()))
        {
            advance();
        }

        StringView value_no_decimal = content.add(begin).slice(position - begin);
        StringView value_decimal = {};
        usize decimal_begin = 0;
        if(current() == '.')
        {
            is_real = true;
            advance();
            decimal_begin = position;

            while(is_digit(current()))
            {
                advance();
            }
            value_decimal = content.add(decimal_begin).slice(position - decimal_begin);
        }
        
        bool is_unsigned = false;
        if(current() == 'u')
        {
            is_unsigned = true;
        }

        Value v{ValueType::Int, {}};
        for(usize i = 0; i < value_no_decimal.len; i++)
        {
            char c = value_no_decimal[i];

            if(is_unsigned && !is_real)
            {
                v.uinteger = v.uinteger * 10 + (c - '0');
            }
            else
            {
                v.integer = v.uinteger * 10 + (c - '0');
            }
        }

        // decimal
        if(is_real)
        {
            f64 real = v.integer;
            v.floating = real;
            
            f64 decimal = 0.0;
            f64 scale = 1.0;
            for(usize i = 0; i < value_decimal.len; i++)
            {
                char c = value_decimal[i];
                decimal = decimal * 10 + (c - '0');
                scale *= 10;
            }

            v.floating += decimal / scale;
        }

        if(is_real)
        {
            v.type = ValueType::Float;
        }
        else if(is_unsigned)
        {
            v.type = ValueType::UInt;
        }
        else
        {
            v.type = ValueType::Int;
        }
        return v;
    }

    Value read_string()
    {
        advance(); // "
        usize begin = position;

        while(current() != '"' && current() != '\0')
        {
            advance();
        }

        StringView str = content.add(begin).slice(position - begin);
        advance(); // "

        return Value(ValueType::String, {.string = str});
    }

    Value read_value()
    {
        skip_whitespace_and_comments();

        if(is_digit(current()))
        {
            return read_number();
        }
        else if(same_as_and_advance("true"))
        {
            return Value(ValueType::Bool, {.boolean = true});
        }
        else if(same_as_and_advance("false"))
        {
            return Value(ValueType::Bool, {.boolean = false});
        }
        else if(current() == '"')
        {
            return read_string();
        }

        return Value(ValueType::Null, {});
    }

    void read_segment(Segment& segment)
    {
        StringView identifier = {};
        if(is_identifier_start(current()))
        {
            identifier = read_identifier();
        }

        skip_whitespace_and_comments();

        if(current() == '=') // value
        {
            advance();
            skip_whitespace_and_comments();
            Value value = read_value();
            segment.values.emplace(identifier, value);
        }
        else if(current() == '"')
        {
            Value string_name = read_string();
            skip_whitespace_and_comments();

            if(current() == '{')
            {
                Segment& new_segment = segment.segments.emplace(
                    identifier, allocator, identifier, string_name.string);
                advance();
                while(current() != '}' && current() != '\0')
                {
                    skip_whitespace_and_comments();
                    read_segment(new_segment);
                }

                advance(); // }
            }
        }
        else if(current() == '{')
        {
            Segment& new_segment = segment.segments.emplace(
                identifier, allocator, identifier, "");
            advance();
            while(current() != '}' && current() != '\0')
            {
                skip_whitespace_and_comments();
                read_segment(new_segment);
            }

            advance(); // }   
        }
    }
};

void Parser::parse(Mem::Allocator& allocator, StringView content, Segment& segment)
{
    Reader reader{allocator, content};

    while(reader.position < content.len)
    {
        reader.skip_whitespace_and_comments();
        reader.read_segment(segment);
    }
}

Document::Document(Mem::Allocator& allocator, StringView path)
: data(allocator), allocator(allocator)
{
    if(!IO::File::exists(allocator, path))
    {
        return;
    }

    content = IO::File::read_all(allocator, path);
    Parser::parse(allocator, Mem::from_bytes<char>(content), data.global_segment);
}

Document::~Document()
{
    if(content.ptr() != nullptr)
    {
        allocator.free(content);
    }
}

}
