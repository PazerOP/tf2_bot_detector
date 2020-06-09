/*
  Copyright (c) 2018 nullworks. All rights reserved.
*/

#include <menu/object/TabButton.hpp>

static settings::RVariable<rgba_t> color_selected{ "zk.style.tab-button.color.selected.background", "446498ff" };
static settings::RVariable<rgba_t> color_separator{ "zk.style.tab-button.color.separator", "446498ff" };
static settings::RVariable<rgba_t> color_hover_underline{ "zk.style.tab-button.color.hover.underline", "446498ff" };
static settings::RVariable<rgba_t> color_selected_underline{ "zk.style.tab-button.color.selected.underline", "446498ff" };

static settings::RVariable<rgba_t> color_text_selected{ "zk.style.tab-button.color.text.selected", "ffffff" };
static settings::RVariable<rgba_t> color_text{ "zk.style.tab-button.color.text.inactive", "cccccc" };

namespace zerokernel
{

TabButton::TabButton(TabSelection &parent, size_t id) : BaseMenuObject{}, parent(parent), id(id)
{
    setParent(&parent);
    // FIXME height
    bb.resize(-1, 14);
    text.setParent(this);
    text.move(0, 0);
    text.bb.setPadding(2, 0, 6, 6);
    text.bb.width.setContent();
    text.bb.height.setFill();
    text.set(parent.options.at(id));
    bb.width.setContent();
}

void TabButton::render()
{
    bool selected = (parent.active == id);
    if (selected)
    {
        renderBackground(*color_selected);
        // TODO magic numbers?
        draw::Line(bb.getBorderBox().x + 6, bb.getBorderBox().bottom() - 3, bb.getBorderBox().width - 12, 0, *color_selected_underline, 1);
    }
    else if (isHovered())
    {
        draw::Line(bb.getBorderBox().x + 6, bb.getBorderBox().bottom() - 3, bb.getBorderBox().width - 12, 0, *color_hover_underline, 1);
    }

    draw::Line(bb.getBorderBox().right(), bb.getBorderBox().top() - 1, 0, bb.getBorderBox().height, *color_separator, 1);

    text.setColorText(selected ? &*color_text_selected : &*color_text);
    text.render();
}

bool TabButton::onLeftMouseClick()
{
    parent.selectTab(id);
    BaseMenuObject::onLeftMouseClick();
    return true;
}

void TabButton::recalculateSize()
{
    BaseMenuObject::recalculateSize();

    bb.shrinkContent();
    bb.extend(text.getBoundingBox());
}

void TabButton::onMove()
{
    BaseMenuObject::onMove();

    text.onParentMove();
}

void TabButton::recursiveSizeUpdate()
{
    BaseMenuObject::recursiveSizeUpdate();

    text.recursiveSizeUpdate();
}
} // namespace zerokernel
