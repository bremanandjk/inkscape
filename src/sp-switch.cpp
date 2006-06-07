#define __SP_SWITCH_CPP__

/*
 * SVG <switch> implementation
 *
 * Authors:
 *   Andrius R. <knutux@gmail.com>
 *
 * Copyright (C) 2006 authors
 *
 * Released under GNU GPL, read the file 'COPYING' for more information
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <glibmm/i18n.h>

#include "sp-switch.h"
#include "display/nr-arena-group.h"
#include "conditions.h"

static void sp_switch_class_init (SPSwitchClass *klass);
static void sp_switch_init (SPSwitch *group);

static SPGroupClass * parent_class;

GType CSwitch::getType (void)
{
	static GType switch_type = 0;
	if (!switch_type) {
		GTypeInfo switch_info = {
			sizeof (SPSwitchClass),
			NULL,	/* base_init */
			NULL,	/* base_finalize */
			(GClassInitFunc) sp_switch_class_init,
			NULL,	/* class_finalize */
			NULL,	/* class_data */
			sizeof (SPSwitch),
			16,	/* n_preallocs */
             (GInstanceInitFunc) sp_switch_init,
			NULL,	/* value_table */
		};
		switch_type = g_type_register_static (SP_TYPE_GROUP, "SPSwitch", &switch_info, (GTypeFlags)0);
	}
	return switch_type;
}

static void
sp_switch_class_init (SPSwitchClass *) {
    parent_class = (SPGroupClass *)g_type_class_ref (SP_TYPE_GROUP);
}

static void sp_switch_init (SPSwitch *group)
{
    if (group->group)
        delete group->group;

    group->group = new CSwitch(group);
}

CSwitch::CSwitch(SPGroup *group) : CGroup(group), _cached_item(NULL), _release_handler_id(0) {
}

CSwitch::~CSwitch() {
    _releaseLastItem(_cached_item);
}

SPObject *CSwitch::_evaluateFirst() {
    for (SPObject *child = sp_object_first_child(_group) ; child != NULL ; child = SP_OBJECT_NEXT(child) ) {
        if (SP_IS_ITEM(child) && sp_item_evaluate(SP_ITEM(child)))
            return child;
    }
    return NULL;
}

GSList *CSwitch::_childList(bool add_ref, Action action) {
    if ( ActionGeneral != action ) {
        return CGroup::_childList(add_ref, action);
    }

    SPObject *child = _evaluateFirst();
    if (NULL == child)
        return NULL;

    if (add_ref)
        g_object_ref (G_OBJECT (child));

    return g_slist_prepend (NULL, child);
}

gchar *CSwitch::getDescription() {
    gint len = getItemCount();
    return g_strdup_printf(
            ngettext("<b>Conditional group</b> of <b>%d</b> object",
                 "<b>Conditional group</b> of <b>%d</b> objects",
                 len), len);
}

void CSwitch::onChildAdded(Inkscape::XML::Node *) {
    _reevaluate(true);
}

void CSwitch::onChildRemoved(Inkscape::XML::Node *) {
    _reevaluate();
}

void CSwitch::onOrderChanged (Inkscape::XML::Node *, Inkscape::XML::Node *, Inkscape::XML::Node *)
{
    _reevaluate();
}

void CSwitch::_reevaluate(bool add_to_arena) {
    SPObject *evaluated_child = _evaluateFirst();
    if (!evaluated_child || _cached_item == evaluated_child) {
        return;
    }

    _releaseLastItem(_cached_item);

    SPItem * child;
    for ( GSList *l = _childList(false, ActionShow);
            NULL != l ; l = g_slist_remove (l, l->data))
    {
        SPObject *o = SP_OBJECT (l->data);
        if ( !SP_IS_ITEM (o) ) {
            continue;
        }

        child = SP_ITEM (o);
        child->setEvaluated(o == evaluated_child);
    }

    _cached_item = evaluated_child;
    _release_handler_id = g_signal_connect(
                                    G_OBJECT(evaluated_child), "release",
                                    G_CALLBACK(&CSwitch::_releaseItem),
                                    this);

    _group->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
}

void CSwitch::_releaseItem(SPObject *obj, CSwitch *selection)
{
    selection->_releaseLastItem(obj);
}

void CSwitch::_releaseLastItem(SPObject *obj)
{
    if (NULL == _cached_item || _cached_item != obj)
        return;

    g_signal_handler_disconnect(G_OBJECT(_cached_item), _release_handler_id);
    _release_handler_id = 0;
    _cached_item = NULL;
}

void CSwitch::_showChildren (NRArena *arena, NRArenaItem *ai, unsigned int key, unsigned int flags) {
    SPObject *evaluated_child = _evaluateFirst();

    NRArenaItem *ac = NULL;
    NRArenaItem *ar = NULL;
    SPItem * child;
    GSList *l = _childList(false, ActionShow);
    while (l) {
        SPObject *o = SP_OBJECT (l->data);
        if (SP_IS_ITEM (o)) {
            child = SP_ITEM (o);
            child->setEvaluated(o == evaluated_child);
            ac = sp_item_invoke_show (child, arena, key, flags);
            if (ac) {
                nr_arena_item_add_child (ai, ac, ar);
                ar = ac;
                nr_arena_item_unref (ac);
            }
        }
        l = g_slist_remove (l, o);
    }
}
