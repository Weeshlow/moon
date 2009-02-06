/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * panel.h
 *
 * Copyright 2007 Novell, Inc. (http://www.novell.com)
 *
 * See the LICENSE file included with the distribution for details.
 * 
 */

#ifndef __MOON_PANEL_H__
#define __MOON_PANEL_H__

#include <glib.h>
#include "frameworkelement.h"

/* @ContentProperty="Children" */
/* @Namespace=System.Windows.Controls */
class Panel : public FrameworkElement {
 private:
	//
	// Contains the last element where the mouse entered
	//
	UIElement *mouse_over;

	void ChildAdded (UIElement *child);
	void ChildRemoved (UIElement *child);

 protected:
	virtual ~Panel ();

	virtual bool EmptyBackground ();
 public:
 	/* @PropertyType=Brush,GenerateAccessors */
	static DependencyProperty *BackgroundProperty;
 	/* @PropertyType=UIElementCollection,ManagedFieldAccess=Internal,ManagedSetterAccess=Internal,GenerateAccessors */
	static DependencyProperty *ChildrenProperty;
	
 	/* @GenerateCBinding,GeneratePInvoke,ManagedAccess=Protected */
	Panel ();

	virtual void ComputeBounds ();
	virtual Size MeasureOverride (Size availableSize);

	virtual Rect GetCoverageBounds ();
	virtual void Render (cairo_t *cr, Region *region);

	virtual void HitTest (cairo_t *cr, Rect r, List *uielement_list);

	virtual bool InsideObject (cairo_t *cr, double x, double y);

	virtual bool CanFindElement () { return GetBackground () != NULL; }

	virtual bool IsLayoutContainer () { return true; }

	virtual void ShiftPosition (Point p);
	
	virtual Value *GetDefaultValue (DependencyProperty *property);
	virtual void OnPropertyChanged (PropertyChangedEventArgs *args);
	virtual void OnSubPropertyChanged (DependencyProperty *prop, DependencyObject *obj, PropertyChangedEventArgs *subobj_args);
	
	virtual void OnCollectionChanged (Collection *col, CollectionChangedEventArgs *args);

	//
	// Property Accessors
	//
	void SetBackground (Brush *background);
	Brush *GetBackground ();
	
	void SetChildren (UIElementCollection *children);
	UIElementCollection *GetChildren ();
};

#endif /* __MOON_PANEL_H__ */
