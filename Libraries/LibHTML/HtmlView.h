#pragma once

#include <LibGUI/GScrollableWidget.h>
#include <LibHTML/DOM/Document.h>

class HtmlView : public GScrollableWidget {
    C_OBJECT(HtmlView)
public:
    virtual ~HtmlView() override {}

    Document* document() { return m_document; }
    const Document* document() const { return m_document; }
    void set_document(Document*);

    Function<void(const String&)> on_link_click;
    Function<void(const String&)> on_title_change;

protected:
    HtmlView(GWidget* parent = nullptr);

    virtual void resize_event(GResizeEvent&) override;
    virtual void paint_event(GPaintEvent&) override;
    virtual void mousemove_event(GMouseEvent&) override;
    virtual void mousedown_event(GMouseEvent&) override;

private:
    void layout_and_sync_size();

    RefPtr<Document> m_document;
    RefPtr<LayoutNode> m_layout_root;
};
