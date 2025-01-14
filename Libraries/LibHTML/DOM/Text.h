#pragma once

#include <AK/String.h>
#include <LibHTML/DOM/Node.h>

class Text final : public Node {
public:
    explicit Text(Document&, const String&);
    virtual ~Text() override;

    const String& data() const { return m_data; }

    virtual String tag_name() const override { return "#text"; }

    virtual String text_content() const override { return m_data; }

private:
    String m_data;
};
