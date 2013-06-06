/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Zsombor Egri <zsombor.egri@canonical.com>
 */

#include "ullayouts.h"
#include "ullayouts_p.h"
#include "ullayoutfragment.h"
#include "ulconditionallayout.h"
#include "layoutaction_p.h"
#include <QtQml/QQmlInfo>

ULLayoutsPrivate::ULLayoutsPrivate(ULLayouts *qq)
    : QQmlIncubator(AsynchronousIfNested)
    , q_ptr(qq)
    , currentLayoutItem(0)
    , currentLayoutIndex(-1)
    , ready(false)
{
}


/******************************************************************************
 * QQmlListProperty functions
 */
void ULLayoutsPrivate::append_layout(QQmlListProperty<ULConditionalLayout> *list, ULConditionalLayout *layout)
{
    ULLayouts *_this = static_cast<ULLayouts*>(list->object);
    if (layout) {
        layout->setParent(_this);
        _this->d_ptr->layouts.append(layout);
    }
}

int ULLayoutsPrivate::count_layouts(QQmlListProperty<ULConditionalLayout> *list)
{
    ULLayouts *_this = static_cast<ULLayouts*>(list->object);
    return _this->d_ptr->layouts.count();
}

ULConditionalLayout *ULLayoutsPrivate::at_layout(QQmlListProperty<ULConditionalLayout> *list, int index)
{
    ULLayouts *_this = static_cast<ULLayouts*>(list->object);
    return _this->d_ptr->layouts.at(index);
}

void ULLayoutsPrivate::clear_layouts(QQmlListProperty<ULConditionalLayout> *list)
{
    ULLayouts *_this = static_cast<ULLayouts*>(list->object);
    _this->d_ptr->layouts.clear();
}


/******************************************************************************
 * ULLayoutsPrivate also acts as QQmlIncubator for the dynamically created layouts.
 * QQmlIncubator stuff
 */
void ULLayoutsPrivate::setInitialState(QObject *object)
{
    Q_Q(ULLayouts);
    // object context's parent is the creation context; link it to the object so we
    // delete them together
    qmlContext(object)->parentContext()->setParent(object);
    // set parent
    object->setParent(q);
    QQuickItem *item = static_cast<QQuickItem*>(object);
    item->setParentItem(q);
//    item->setVisible(false);
    item->setEnabled(true);
}

/*
 * Called upon QQmlComponent::create() to notify the status of the component
 * creation.
 */
void ULLayoutsPrivate::statusChanged(Status status)
{
    Q_Q(ULLayouts);
    if (status == Ready) {
        // complete layouting
        QQuickItem *previousLayout = currentLayoutItem;

        // reset the layout
        currentLayoutItem = qobject_cast<QQuickItem*>(object());
        Q_ASSERT(currentLayoutItem);

        changes.addChange(new PropertyChange(currentLayoutItem, "anchors.fill", qVariantFromValue(q), PropertyChange::High));

        // hide all non-laid out items first
        hideExcludedItems();

        //reparent components to be laid out
        reparentItems();
        // enable and show layout
//        changes.addChange(new PropertyChange(currentLayoutItem, "enabled", true))
//               .addChange(new PropertyChange(currentLayoutItem, "visible", true));
        // apply changes
        changes.apply();
        // clear previous layout
        delete previousLayout;

        Q_EMIT q->currentLayoutChanged();
    } else if (status == Error) {
        Q_Q(ULLayouts);
        qmlInfo(q, errors());
    }
}

void ULLayoutsPrivate::hideExcludedItems()
{
    for (int i = 0; i < excludedFromLayout.count(); i++) {
        changes.addChange(new PropertyChange(excludedFromLayout[i], "visible", false))
               .addChange(new PropertyChange(excludedFromLayout[i], "enabled", false));
    }
}

/*
 * Re-parent items to the new layout.
 */
void ULLayoutsPrivate::reparentItems()
{
    // create copy of items list, to keep track of which ones we change
    LaidOutItemsMap unusedItems = itemsToLayout;

    // iterate through the Layout definition to find containers - those Items with
    // ConditionalLayout.items set
    QList<QQuickItem*> items = currentLayoutItem->findChildren<QQuickItem*>();
    // check also root element, as it may also be marked as layout section
    items.prepend(currentLayoutItem);

    Q_FOREACH(QQuickItem *container, items) {
        // check whether we have LayoutFragment declared
        ULLayoutFragment *layoutFragment = qobject_cast<ULLayoutFragment*>(container);
        if (layoutFragment) {
            reparentToLayoutFragment(unusedItems, layoutFragment);
        } else {
            ULConditionalLayoutAttached *fragment = qobject_cast<ULConditionalLayoutAttached*>(
                        qmlAttachedPropertiesObject<ULConditionalLayout>(container, false));
            if (!fragment || fragment->items().isEmpty()) {
                continue;
            }
            reparentToConditionalLayout(unusedItems, container, fragment);
        }
    }

    // hide the rest of the unused ones
    QHashIterator<QString, QQuickItem*> i(unusedItems);
    while (i.hasNext()) {
        i.next();
        changes.addChange(new PropertyChange(i.value(), "visible", false))
               .addChange(new PropertyChange(i.value(), "enabled", false));
    }
}

/*
 * Re-parent to LayoutFragment.
 */
void ULLayoutsPrivate::reparentToLayoutFragment(LaidOutItemsMap &map, ULLayoutFragment *fragment)
{
    QString itemName = fragment->item();
    if (itemName.isEmpty()) {
        qmlInfo(fragment) << "Warning: item not specified for LayoutFragment";
        return;
    }

    QQuickItem *item = map.value(itemName);
    if (!item) {
        qmlInfo(fragment) << "Warning: item \"" << itemName
                          << "\" not specified or already laid out";
        return;
    }

    // the component fills the parent
    changes.addChange(new ParentChange(item, fragment))
           .addChange(new ItemStackBackup(item, currentLayoutItem))
           .addChange(new PropertyChange(item, "anchors.fill", qVariantFromValue(fragment)))
               // break and backup anchors
           .addChange(new AnchorBackup(item));

    // remove from unused ones
    map.remove(itemName);
}

/*
 * Re-parent using ConditionalLayout properties
 */
void ULLayoutsPrivate::reparentToConditionalLayout(LaidOutItemsMap &map, QQuickItem *container, ULConditionalLayoutAttached *fragment)
{
    Q_FOREACH(const QString &itemName, fragment->items()) {
        // check if we have this item listed to be laid out
        QQuickItem *item = map.value(itemName);
        if (item != 0) {
            // reparent and break anchors
            changes.addChange(new ParentChange(item, container))
                   .addChange(new ItemStackBackup(item, currentLayoutItem))
                    .addChange(new PropertyChange(item, "enabled", true))
                    // break and backup anchors
                   .addChange(new AnchorBackup(item));


            // special cases
            if (container->inherits("QQuickColumn")) {
                changes.addChange(new PropertyChange(item, "x", 0.0));
            } else if (container->inherits("QQuickRow")) {
                changes.addChange(new PropertyChange(item, "y", 0.0));
            } else if (container->inherits("QQuickFlow") || container->inherits("QQuickGrid")) {
                changes.addChange(new PropertyChange(item, "x", 0.0))
                       .addChange(new PropertyChange(item, "y", 0.0));
            }
            changes.addConditionalProperties(item, fragment);

            // remove from unused ones
            map.remove(itemName);
        } else {
            qmlInfo(fragment->parent()) << "Warning: item \"" << itemName
                              << "\" not specified or already laid out";
        }
    }
}

/*
 * Collect items to be laid out.
 */
void ULLayoutsPrivate::getLaidOutItems()
{
    Q_Q(ULLayouts);

    QList<QQuickItem*> items = q->findChildren<QQuickItem*>();
    for (int i = 0; i < items.count(); i++) {
        QQuickItem *item = items[i];
        ULConditionalLayoutAttached *marker = qobject_cast<ULConditionalLayoutAttached*>(
                    qmlAttachedPropertiesObject<ULConditionalLayout>(item, false));
        if (marker && !marker->item().isEmpty()) {
            itemsToLayout.insert(marker->item(), item);
        } else {
            QQuickItem *pl = item->parentItem();
            marker = 0;
            // check if its parent is included in the layout
            while (pl) {
                marker = qobject_cast<ULConditionalLayoutAttached*>(
                            qmlAttachedPropertiesObject<ULConditionalLayout>(pl, false));
                if (marker && !marker->item().isEmpty()) {
                    break;
                }
                pl = pl->parentItem();
            }
            if (!marker || (marker && marker->item().isEmpty())) {
                // remember theese so we hide them once we switch away from default layout
                excludedFromLayout << item;
            }
        }
    }
}

/*
 * Apply layout change. The new layout creation will be completed in statusChange().
 */
void ULLayoutsPrivate::reLayout()
{
    if (!ready || (currentLayoutIndex < 0)) {
        return;
    }
    if (!layouts[currentLayoutIndex]->layout()) {
        return;
    }

    // redo changes
    changes.revert();
    changes.clear();

    // clear the incubator before using it
    clear();
    QQmlComponent *component = layouts[currentLayoutIndex]->layout();
    // create using incubation as it may be created asynchronously,
    // case when the attached properties are not yet enumerated
    Q_Q(ULLayouts);
    QQmlContext *context = new QQmlContext(qmlContext(q), q);
    component->create(*this, context);
}

/*
 * Updates the current layout.
 */
void ULLayoutsPrivate::updateLayout()
{
    if (!ready) {
        return;
    }

    // go through conditions and re-parent for the first valid one
    for (int i = 0; i < layouts.count(); i++) {
        ULConditionalLayout *layout = layouts[i];
        if (!layout->layoutName().isEmpty() && layout->when() && layout->when()->evaluate().toBool()) {
            if (currentLayoutIndex == i) {
                return;
            }
            currentLayoutIndex = i;
            // update layout
            reLayout();
            return;
        }
    }
    // check if we need to switch back to default layout
    if (currentLayoutIndex >= 0) {
        // revert and clear changes
        changes.revert();
        changes.clear();
        delete currentLayoutItem;
        currentLayoutItem = 0;
        currentLayoutIndex = -1;
    }
}


/*!
 * \qmltype Layouts
 * \instantiates ULLayouts
 * \inqmlmodule Ubuntu.Layouts 0.1
 * \ingroup ubuntu-layouts
 * \brief Layouts component defines the layouts for different form factors and
 * embeds the components to be laid out.
 *
 * Layouts is a layout block component incorporating layout definitions and
 * components to lay out. The layouts are defined in layouts property, which
 * is a list of ConditionalLayout components, each declaring a form of arrangement
 * for the components specified to be laid out.
 *
 * \qml
 * Layouts {
 *     id: layouts
 *     layouts: [
 *         ConditionalLayout {
 *             when: layouts.width > units.gu(60) && layouts.width <= units.gu(100)
 *             Flow {
 *                 //[...]
 *             }
 *         },
 *         ConditionalLayout {
 *             when: layouts.width > units.gu(100)
 *             Flickable {
 *                 contentHeight: column.childrenRect.height
 *                 Column {
 *                     id: column
 *                     //[...]
 *                 }
 *             }
 *         }
 *     ]
 * }
 * \endqml
 *
 * Components wanted to be laid out must be declared as children of Layouts compponent,
 * each having ConditionalLayout.item attached property set to a uniquie string.
 * \b TODO: see also ConditionalItem.item
 *
 * \qml
 * Layouts {
 *     id: layouts
 *     layouts: [
 *         ConditionalLayout {
 *             when: layouts.width > units.gu(60) && layouts.width <= units.gu(100)
 *             Flow {
 *                 //[...]
 *             }
 *         },
 *         ConditionalLayout {
 *             when: layouts.width > units.gu(100)
 *             Flickable {
 *                 contentHeight: column.childrenRect.height
 *                 Column {
 *                     id: column
 *                     //[...]
 *                 }
 *             }
 *         }
 *     ]
 *
 *     Row {
 *         anchors.fill: parent
 *         Button {
 *             text: "Press me"
 *             ConditionalLayout.item: "item1"
 *         }
 *         Button {
 *             text: "Cancel"
 *             ConditionalLayout.item: "item2"
 *         }
 *     }
 * }
 * \endqml
 *
 * Components listed as children of Layouts are considered to form the \a default
 * layout.
 *
 * Layouts defined by ConditionalLayout are created and activated when at least
 * one of the layout's condition is evaluated to true. In which case components
 * marked for layout are re-parented to the components defined to lay out those
 * defined in the ConditionalLayout. In case multiple conditions are evaluated
 * to true, the first one in the list will be activated. The deactivated layout
 * is destroyed, exception being the default layout, which is kept in memory for
 * the entire lifetime of the Layouts component.
 *
 * Upon activation, the created component fills in the entire layout block.
 *
 * \qml
 * Layouts {
 *     id: layouts
 *     layouts: [
 *         ConditionalLayout {
 *             when: layouts.width > units.gu(60) && layouts.width <= units.gu(100)
 *             Flow {
 *                 ConditionalLayout.items: ["item1", "item2"]
 *             }
 *         },
 *         ConditionalLayout {
 *             when: layouts.width > units.gu(100)
 *             Flickable {
 *                 contentHeight: column.childrenRect.height
 *                 Column {
 *                     id: column
 *                     ConditionalLayout.items: ["item2", "item1"]
 *                 }
 *             }
 *         }
 *     ]
 *
 *     Row {
 *         anchors.fill: parent
 *         Button {
 *             text: "Press me"
 *             ConditionalLayout.item: "item1"
 *         }
 *         Button {
 *             text: "Cancel"
 *             ConditionalLayout.item: "item2"
 *         }
 *     }
 * }
 * \endqml
 *
 * \b Note: the order items are laid out in a layout container depends on the order
 * the items are specified in ConditionalLayout.items attached property.
 *
 */

ULLayouts::ULLayouts(QQuickItem *parent):
    QQuickItem(parent),
    d_ptr(new ULLayoutsPrivate(this))
{
}

ULLayouts::~ULLayouts()
{
}

void ULLayouts::componentComplete()
{
    QQuickItem::componentComplete();
    Q_D(ULLayouts);
    d->ready = true;
    d->getLaidOutItems();
    d->updateLayout();
}

/*!
 * \qmlproperty string Layouts::currentLayout
 * The property holds the active layout name. The default layout is identified
 * by an empty string. This property can be used for additional customization
 * of the components which are not supported by the layouting.
 */

QString ULLayouts::currentLayout() const
{
    Q_D(const ULLayouts);
    return d->currentLayoutIndex >= 0 ? d->layouts[d->currentLayoutIndex]->layoutName() : QString();
}

/*!
 * \internal
 * Provides a list of layouts for internal use.
 */
QList<ULConditionalLayout*> ULLayouts::layoutList()
{
    Q_D(ULLayouts);
    return d->layouts;
}

/*!
 * \qmlproperty list<ConditionalLayout> Layouts::layouts
 * The property holds the list of different ConditionalLayout elements.
 */
QQmlListProperty<ULConditionalLayout> ULLayouts::layouts()
{
    Q_D(ULLayouts);
    return QQmlListProperty<ULConditionalLayout>(this, &(d->layouts),
                                                 &ULLayoutsPrivate::append_layout,
                                                 &ULLayoutsPrivate::count_layouts,
                                                 &ULLayoutsPrivate::at_layout,
                                                 &ULLayoutsPrivate::clear_layouts);
}
