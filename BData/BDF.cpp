#include "BData/BDF.hpp"

#include <IO/File.hpp>


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
    Collections::StringView path;
    Collections::StringView content;
    const IO::Writer& err;
    usize position;
    usize line;
    usize column;

    Reader(Mem::Allocator& allocator, Collections::StringView path, Collections::StringView content,
        const IO::Writer& err)
    : allocator(allocator), path(path), content(content), err(err), position(0), line(1), column(1)
    {}

    char current() const { return position < content.len ? content[position] : '\0'; }
    char next(usize offset) const { return position + offset < content.len ? content[position + offset] : '\0'; }
    void advance()
    {
        if(current() == '\n')
        {
            line++;
            column = 1;
        }
        else
        {
            column++;
        }

        position++;
    }

    void simple_error(Collections::StringView msg)
    {
        Format::format<true>(err, "{}({},{}):{}", path, line, column, msg);
    }

    void expected(char character)
    {
        if(current() == character)
        {
            advance();
            return;
        }

        Collections::StringView character_str{&character, 1};
        Format::format<true>(err, "{}({},{}):'{}' was expected", path, line, column, character_str);
    }

    bool can_be_a_number()
    {
        return is_digit(current()) || (current() == '-' && is_digit(next(1)));
    }

    bool same_as_and_advance(Collections::StringView id)
    {
        Collections::StringView next_slice = content.add(position).slice(id.len);
        if(!next_slice.equals(id))
        {
            return false;
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

    void skip_whitespace()
    {
        while(current() == ' '
            || current() == '\t'
            || current() == '\r'
            || current() == '\n')
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

    Collections::StringView read_identifier()
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

        bool is_negative = current() == '-';
        if(is_negative)
        {
            advance();
        }

        usize begin = position;
        while(is_digit(current()))
        {
            advance();
        }

        Collections::StringView value_no_decimal = content.add(begin).slice(position - begin);
        Collections::StringView value_decimal = {};
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
        if(current() == 'u' || current() == 'U')
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

            if(is_negative)
            {
                v.integer = -v.integer;
            }
        }

        // decimal
        if(is_real)
        {
            f64 real = Math::abs(v.integer);
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
            if(is_negative)
            {
                v.floating = -v.floating;
            }
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
        expected('"');
        usize begin = position;

        while(current() != '"' && current() != '\0')
        {
            advance();
        }

        Collections::StringView str = content.add(begin).slice(position - begin);
        expected('"');

        return Value(ValueType::String, {.string = str});
    }

    Value read_vector(const u32 component_count)
    {
        DebugAssert(component_count > 1 && component_count <= 4, "invalid component count");
        u32 encountered_components = 0;

        expected('(');
        skip_whitespace();

        Value vector{ValueType::Vector2, {.vec2 = {}}};
        if(component_count == 3)
        {
            vector.type = ValueType::Vector3;
        }
        else if(component_count == 4)
        {
            vector.type = ValueType::Vector4;
        }

        while(can_be_a_number()
            && current() != '\0'
            && encountered_components < component_count)
        {            
            Value component = read_number();
            if(component.type == ValueType::UInt)
            {
                return Value{ValueType(-1), {}};
            }

            if(component.type == ValueType::Int)
            {
                vector.vec4[encountered_components] = component.integer;
            }
            if(component.type == ValueType::Float)
            {
                vector.vec4[encountered_components] = component.floating;
            }

            encountered_components++;
            skip_whitespace();
            if(current() == ',')
            {
                advance();
            }
            skip_whitespace();
        }

        expected(')');

        return vector;
    }

    Value read_array(Segment& segment)
    {
        expected('[');

        Value value{ValueType::Array, {}};
        usize begin = segment.arrays.count;

        while(current() != ']')
        {
            skip_whitespace();
            Value new_value = read_value(segment);
            (void)segment.arrays.add(new_value);
            skip_whitespace();

            if(current() == '\0') // exceptional case
            {
                return Value(ValueType::Null, {});
            }
            else if(current() == ',')
            {
                advance();
            }
        }

        value.array.begin = begin;
        value.array.end = segment.arrays.count;
        return value;
    }

    Value read_value(Segment& segment)
    {
        skip_whitespace();

        if(can_be_a_number())
        {
            return read_number();
        }
        else if(same_as_and_advance("null"))
        {
            return Value(ValueType::Null, {});
        }
        else if(same_as_and_advance("true"))
        {
            return Value(ValueType::Bool, {.boolean = true});
        }
        else if(same_as_and_advance("false"))
        {
            return Value(ValueType::Bool, {.boolean = false});
        }
        else if(same_as_and_advance("Vec2"))
        {
            return read_vector(2);
        }
        else if(same_as_and_advance("Vec3"))
        {
            return read_vector(3);
        }
        else if(same_as_and_advance("Vec4"))
        {
            return read_vector(4);
        }
        else if(same_as_and_advance("Rect2D"))
        {
            return read_vector(4);
        }
        else if(current() == '"')
        {
            return read_string();
        }
        else if(current() == '[')
        {
            return read_array(segment);
        }

        return Value(ValueType(-1), {});
    }

    void read_segment(Segment& segment)
    {
        skip_whitespace_and_comments();

        Collections::StringView identifier = read_identifier();
        if(identifier.len == 0)
        {
            simple_error("an identifier was expected");
        }

        skip_whitespace();

        if(current() == '=') // value
        {
            advance();
            skip_whitespace();
            Value value = read_value(segment);
            segment.values.emplace(identifier, value);
        }
        else if(current() == '"')
        {
            Value string_name = read_string();
            skip_whitespace();

            if(current() == '{')
            {
                Segment& new_segment = segment.segments.emplace(allocator, identifier, string_name.string);
                advance();

                while(current() != '}')
                {
                    skip_whitespace_and_comments();
                    read_segment(new_segment);
                    advance_until_new_line(); // a statement should be one line
                    skip_whitespace_and_comments();

                    if(current() == '\0') // exceptional case
                    {
                        return;
                    }
                }

                expected('}');
            }
        }
        else if(current() == '{')
        {
            // Anonymus segment
            Segment& new_segment = segment.segments.emplace(allocator, identifier, "");
            advance();

            while(current() != '}')
            {
                skip_whitespace_and_comments();
                read_segment(new_segment);
                advance_until_new_line(); // a statement should be one line
                skip_whitespace_and_comments();

                if(current() == '\0') // exceptional case
                {
                    return;
                }
            }

            expected('}');
        }
        else
        {
            if(current() == '\0')
            {
                return;
            }
            simple_error("invalid expression");
        }
    }
};

void Parser::parse(Mem::Allocator& allocator, Collections::StringView path, Collections::StringView content,
    Segment& segment, const IO::Writer& err)
{
    Reader reader{allocator, path, content, err};

    while(reader.position < content.len)
    {
        reader.skip_whitespace_and_comments();
        reader.read_segment(segment);
        reader.skip_whitespace_and_comments();
    }
}

Document::Document(Mem::Allocator& allocator, Collections::StringView path, const IO::Writer& err)
: data(allocator)
{
    if(!IO::File::exists(allocator, path))
    {
        return;
    }

    data.content = IO::File::read_all(allocator, path);
    Parser::parse(allocator, path, Collections::StringView(Mem::from_bytes<const char>(data.content)),
        data.global_segment, err);
}

Document::~Document()
{
    if(data.content.ptr() != nullptr)
    {
        data.allocator.free(data.content);
    }
}

}
