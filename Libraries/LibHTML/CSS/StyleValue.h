#pragma once

#include <AK/RefCounted.h>
#include <AK/RefPtr.h>
#include <AK/String.h>
#include <AK/StringView.h>
#include <LibDraw/Color.h>
#include <LibHTML/CSS/Length.h>

class StyleValue : public RefCounted<StyleValue> {
public:
    virtual ~StyleValue();

    enum class Type {
        Invalid,
        Inherit,
        Initial,
        String,
        Length,
        Color,
    };

    Type type() const { return m_type; }

    bool is_inherit() const { return type() == Type::Inherit; }
    bool is_initial() const { return type() == Type::Initial; }
    bool is_color() const { return type() == Type::Color; }

    virtual String to_string() const = 0;
    virtual Length to_length() const { return {}; }
    virtual Color to_color() const { return {}; }

    virtual bool is_auto() const { return false; }

protected:
    explicit StyleValue(Type);

private:
    Type m_type { Type::Invalid };
};

class StringStyleValue : public StyleValue {
public:
    static NonnullRefPtr<StringStyleValue> create(const String& string)
    {
        return adopt(*new StringStyleValue(string));
    }
    virtual ~StringStyleValue() override {}

    String to_string() const override { return m_string; }

private:
    explicit StringStyleValue(const String& string)
        : StyleValue(Type::String)
        , m_string(string)
    {
    }

    String m_string;
};

class LengthStyleValue : public StyleValue {
public:
    static NonnullRefPtr<LengthStyleValue> create(const Length& length)
    {
        return adopt(*new LengthStyleValue(length));
    }
    virtual ~LengthStyleValue() override {}

    virtual String to_string() const override { return m_length.to_string(); }
    virtual Length to_length() const override { return m_length; }

    const Length& length() const { return m_length; }

    virtual bool is_auto() const override { return m_length.is_auto(); }

private:
    explicit LengthStyleValue(const Length& length)
        : StyleValue(Type::Length)
        , m_length(length)
    {
    }

    Length m_length;
};

class InitialStyleValue final : public StyleValue {
public:
    static NonnullRefPtr<InitialStyleValue> create() { return adopt(*new InitialStyleValue); }
    virtual ~InitialStyleValue() override {}

    String to_string() const override { return "initial"; }

private:
    InitialStyleValue()
        : StyleValue(Type::Initial)
    {
    }
};

class InheritStyleValue final : public StyleValue {
public:
    static NonnullRefPtr<InheritStyleValue> create() { return adopt(*new InheritStyleValue); }
    virtual ~InheritStyleValue() override {}

    String to_string() const override { return "inherit"; }

private:
    InheritStyleValue()
        : StyleValue(Type::Inherit)
    {
    }
};

class ColorStyleValue : public StyleValue {
public:
    static NonnullRefPtr<ColorStyleValue> create(Color color)
    {
        return adopt(*new ColorStyleValue(color));
    }
    virtual ~ColorStyleValue() override {}

    Color color() const { return m_color; }
    String to_string() const override { return String::format("COLOR: %s", m_color.to_string().characters()); }
    Color to_color() const override { return m_color; }

private:
    explicit ColorStyleValue(Color color)
        : StyleValue(Type::Color)
        , m_color(color)
    {
    }

    Color m_color;
};
