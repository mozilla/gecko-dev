/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.menu;

import java.util.ArrayList;
import java.util.List;

import org.mozilla.gecko.AppConstants.Versions;
import org.mozilla.gecko.R;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageButton;
import android.widget.LinearLayout;

public class MenuItemActionView extends LinearLayout
                                implements GeckoMenuItem.Layout,
                                           View.OnClickListener {
    private final MenuItemDefault mMenuItem;
    private final MenuItemActionBar mMenuButton;
    private final List<ImageButton> mActionButtons;
    private final List<View.OnClickListener> mActionButtonListeners = new ArrayList<View.OnClickListener>();

    public MenuItemActionView(Context context) {
        this(context, null);
    }

    public MenuItemActionView(Context context, AttributeSet attrs) {
        this(context, attrs, R.attr.menuItemActionViewStyle);
    }

    @TargetApi(14)
    public MenuItemActionView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(R.layout.menu_item_action_view, this);
        mMenuItem = (MenuItemDefault) findViewById(R.id.menu_item);
        mMenuButton = (MenuItemActionBar) findViewById(R.id.menu_item_button);
        mActionButtons = new ArrayList<ImageButton>();
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        final int width = right - left;

        final View parent = (View) getParent();
        final int parentPadding = parent.getPaddingLeft() + parent.getPaddingRight();
        final int horizontalSpaceAvailableInParent = parent.getMeasuredWidth() - parentPadding;

        // Check if there is another View sharing horizontal
        // space with this View in the parent.
        if (width < horizontalSpaceAvailableInParent || mActionButtons.size() != 0) {
            // Use the icon.
            mMenuItem.setVisibility(View.GONE);
            mMenuButton.setVisibility(View.VISIBLE);
        } else {
            // Use the button.
            mMenuItem.setVisibility(View.VISIBLE);
            mMenuButton.setVisibility(View.GONE);
        }

        super.onLayout(changed, left, top, right, bottom);
    }

    @Override
    public void initialize(GeckoMenuItem item) {
        if (item == null) {
            return;
        }

        mMenuItem.initialize(item);
        mMenuButton.initialize(item);
        setEnabled(item.isEnabled());
    }

    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        mMenuItem.setEnabled(enabled);
        mMenuButton.setEnabled(enabled);

        for (ImageButton button : mActionButtons) {
             button.setEnabled(enabled);
             button.setAlpha(enabled ? 255 : 99);
        }
    }

    public void setMenuItemClickListener(View.OnClickListener listener) {
        mMenuItem.setOnClickListener(listener);
        mMenuButton.setOnClickListener(listener);
    }

    public void setMenuItemLongClickListener(View.OnLongClickListener listener) {
        mMenuItem.setOnLongClickListener(listener);
        mMenuButton.setOnLongClickListener(listener);
    }

    public void addActionButtonClickListener(View.OnClickListener listener) {
        mActionButtonListeners.add(listener);
    }

    @Override
    public void setShowIcon(boolean show) {
        mMenuItem.setShowIcon(show);
    }

    public void setIcon(Drawable icon) {
        mMenuItem.setIcon(icon);
        mMenuButton.setIcon(icon);
    }

    public void setIcon(int icon) {
        mMenuItem.setIcon(icon);
        mMenuButton.setIcon(icon);
    }

    public void setTitle(CharSequence title) {
        mMenuItem.setTitle(title);
        mMenuButton.setContentDescription(title);
    }

    public void setSubMenuIndicator(boolean hasSubMenu) {
        mMenuItem.setSubMenuIndicator(hasSubMenu);
    }

    public void addActionButton(Drawable drawable, CharSequence label) {
        // If this is the first icon, retain the text.
        // If not, make the menu item an icon.
        final int count = mActionButtons.size();
        mMenuItem.setVisibility(View.GONE);
        mMenuButton.setVisibility(View.VISIBLE);

        if (drawable != null) {
            ImageButton button = new ImageButton(getContext(), null, R.attr.menuItemShareActionButtonStyle);
            button.setImageDrawable(drawable);
            button.setContentDescription(label);
            button.setOnClickListener(this);
            button.setTag(count);

            final int height = (int) (getResources().getDimension(R.dimen.menu_item_row_height));
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, height);
            params.weight = 1.0f;
            button.setLayoutParams(params);

            // Fill in the action-buttons to the left of the actual menu button.
            mActionButtons.add(button);
            addView(button, count);
        }
    }

    protected int getActionButtonCount() {
        return mActionButtons.size();
    }

    @Override
    public void onClick(View view) {
        for (View.OnClickListener listener : mActionButtonListeners) {
            listener.onClick(view);
        }
    }

    /**
     * Update the styles if this view is being used in the context menus.
     *
     * Ideally, we just use different layout files and styles to set this, but
     * MenuItemActionView is too integrated into GeckoActionProvider to provide
     * an easy separation so instead I provide this hack. I'm sorry.
     */
    public void initContextMenuStyles() {
        final int defaultContextMenuPadding = getContext().getResources().getDimensionPixelOffset(
                R.dimen.context_menu_item_horizontal_padding);
        mMenuItem.setPadding(defaultContextMenuPadding, getPaddingTop(),
                defaultContextMenuPadding, getPaddingBottom());
    }
}
