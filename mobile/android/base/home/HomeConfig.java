/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.home;

import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoEvent;
import org.mozilla.gecko.R;
import org.mozilla.gecko.util.ThreadUtils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

public final class HomeConfig {
    /**
     * Used to determine what type of HomeFragment subclass to use when creating
     * a given panel. With the exception of DYNAMIC, all of these types correspond
     * to a default set of built-in panels. The DYNAMIC panel type is used by
     * third-party services to create panels with varying types of content.
     */
    public static enum PanelType implements Parcelable {
        TOP_SITES("top_sites", TopSitesPanel.class),
        BOOKMARKS("bookmarks", BookmarksPanel.class),
        HISTORY("history", HistoryPanel.class),
        READING_LIST("reading_list", ReadingListPanel.class),
        RECENT_TABS("recent_tabs", RecentTabsPanel.class),
        DYNAMIC("dynamic", DynamicPanel.class);

        private final String mId;
        private final Class<?> mPanelClass;

        PanelType(String id, Class<?> panelClass) {
            mId = id;
            mPanelClass = panelClass;
        }

        public static PanelType fromId(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Could not convert null String to PanelType");
            }

            for (PanelType panelType : PanelType.values()) {
                if (TextUtils.equals(panelType.mId, id.toLowerCase())) {
                    return panelType;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to PanelType");
        }

        @Override
        public String toString() {
            return mId;
        }

        public Class<?> getPanelClass() {
            return mPanelClass;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<PanelType> CREATOR = new Creator<PanelType>() {
            @Override
            public PanelType createFromParcel(final Parcel source) {
                return PanelType.values()[source.readInt()];
            }

            @Override
            public PanelType[] newArray(final int size) {
                return new PanelType[size];
            }
        };
    }

    public static class PanelConfig implements Parcelable {
        private final PanelType mType;
        private final String mTitle;
        private final String mId;
        private final LayoutType mLayoutType;
        private final List<ViewConfig> mViews;
        private final AuthConfig mAuthConfig;
        private final EnumSet<Flags> mFlags;

        private static final String JSON_KEY_TYPE = "type";
        private static final String JSON_KEY_TITLE = "title";
        private static final String JSON_KEY_ID = "id";
        private static final String JSON_KEY_LAYOUT = "layout";
        private static final String JSON_KEY_VIEWS = "views";
        private static final String JSON_KEY_AUTH_CONFIG = "authConfig";
        private static final String JSON_KEY_DEFAULT = "default";
        private static final String JSON_KEY_DISABLED = "disabled";

        public enum Flags {
            DEFAULT_PANEL,
            DISABLED_PANEL
        }

        public PanelConfig(JSONObject json) throws JSONException, IllegalArgumentException {
            final String panelType = json.optString(JSON_KEY_TYPE, null);
            if (TextUtils.isEmpty(panelType)) {
                mType = PanelType.DYNAMIC;
            } else {
                mType = PanelType.fromId(panelType);
            }

            mTitle = json.getString(JSON_KEY_TITLE);
            mId = json.getString(JSON_KEY_ID);

            final String layoutTypeId = json.optString(JSON_KEY_LAYOUT, null);
            if (layoutTypeId != null) {
                mLayoutType = LayoutType.fromId(layoutTypeId);
            } else {
                mLayoutType = null;
            }

            final JSONArray jsonViews = json.optJSONArray(JSON_KEY_VIEWS);
            if (jsonViews != null) {
                mViews = new ArrayList<ViewConfig>();

                final int viewCount = jsonViews.length();
                for (int i = 0; i < viewCount; i++) {
                    final JSONObject jsonViewConfig = (JSONObject) jsonViews.get(i);
                    final ViewConfig viewConfig = new ViewConfig(i, jsonViewConfig);
                    mViews.add(viewConfig);
                }
            } else {
                mViews = null;
            }

            final JSONObject jsonAuthConfig = json.optJSONObject(JSON_KEY_AUTH_CONFIG);
            if (jsonAuthConfig != null) {
                mAuthConfig = new AuthConfig(jsonAuthConfig);
            } else {
                mAuthConfig = null;
            }

            mFlags = EnumSet.noneOf(Flags.class);

            if (json.optBoolean(JSON_KEY_DEFAULT, false)) {
                mFlags.add(Flags.DEFAULT_PANEL);
            }

            if (json.optBoolean(JSON_KEY_DISABLED, false)) {
                mFlags.add(Flags.DISABLED_PANEL);
            }

            validate();
        }

        @SuppressWarnings("unchecked")
        public PanelConfig(Parcel in) {
            mType = (PanelType) in.readParcelable(getClass().getClassLoader());
            mTitle = in.readString();
            mId = in.readString();
            mLayoutType = (LayoutType) in.readParcelable(getClass().getClassLoader());

            mViews = new ArrayList<ViewConfig>();
            in.readTypedList(mViews, ViewConfig.CREATOR);

            mAuthConfig = (AuthConfig) in.readParcelable(getClass().getClassLoader());

            mFlags = (EnumSet<Flags>) in.readSerializable();

            validate();
        }

        public PanelConfig(PanelConfig panelConfig) {
            mType = panelConfig.mType;
            mTitle = panelConfig.mTitle;
            mId = panelConfig.mId;
            mLayoutType = panelConfig.mLayoutType;

            mViews = new ArrayList<ViewConfig>();
            List<ViewConfig> viewConfigs = panelConfig.mViews;
            if (viewConfigs != null) {
                for (ViewConfig viewConfig : viewConfigs) {
                    mViews.add(new ViewConfig(viewConfig));
                }
            }

            mAuthConfig = panelConfig.mAuthConfig;
            mFlags = panelConfig.mFlags.clone();

            validate();
        }

        public PanelConfig(PanelType type, String title, String id) {
            this(type, title, id, EnumSet.noneOf(Flags.class));
        }

        public PanelConfig(PanelType type, String title, String id, EnumSet<Flags> flags) {
            this(type, title, id, null, null, null, flags);
        }

        public PanelConfig(PanelType type, String title, String id, LayoutType layoutType,
                List<ViewConfig> views, AuthConfig authConfig, EnumSet<Flags> flags) {
            mType = type;
            mTitle = title;
            mId = id;
            mLayoutType = layoutType;
            mViews = views;
            mAuthConfig = authConfig;
            mFlags = flags;

            validate();
        }

        private void validate() {
            if (mType == null) {
                throw new IllegalArgumentException("Can't create PanelConfig with null type");
            }

            if (TextUtils.isEmpty(mTitle)) {
                throw new IllegalArgumentException("Can't create PanelConfig with empty title");
            }

            if (TextUtils.isEmpty(mId)) {
                throw new IllegalArgumentException("Can't create PanelConfig with empty id");
            }

            if (mLayoutType == null && mType == PanelType.DYNAMIC) {
                throw new IllegalArgumentException("Can't create a dynamic PanelConfig with null layout type");
            }

            if ((mViews == null || mViews.size() == 0) && mType == PanelType.DYNAMIC) {
                throw new IllegalArgumentException("Can't create a dynamic PanelConfig with no views");
            }

            if (mFlags == null) {
                throw new IllegalArgumentException("Can't create PanelConfig with null flags");
            }
        }

        public PanelType getType() {
            return mType;
        }

        public String getTitle() {
            return mTitle;
        }

        public String getId() {
            return mId;
        }

        public LayoutType getLayoutType() {
            return mLayoutType;
        }

        public int getViewCount() {
            return (mViews != null ? mViews.size() : 0);
        }

        public ViewConfig getViewAt(int index) {
            return (mViews != null ? mViews.get(index) : null);
        }

        public boolean isDynamic() {
            return (mType == PanelType.DYNAMIC);
        }

        public boolean isDefault() {
            return mFlags.contains(Flags.DEFAULT_PANEL);
        }

        private void setIsDefault(boolean isDefault) {
            if (isDefault) {
                mFlags.add(Flags.DEFAULT_PANEL);
            } else {
                mFlags.remove(Flags.DEFAULT_PANEL);
            }
        }

        public boolean isDisabled() {
            return mFlags.contains(Flags.DISABLED_PANEL);
        }

        private void setIsDisabled(boolean isDisabled) {
            if (isDisabled) {
                mFlags.add(Flags.DISABLED_PANEL);
            } else {
                mFlags.remove(Flags.DISABLED_PANEL);
            }
        }

        public AuthConfig getAuthConfig() {
            return mAuthConfig;
        }

        public JSONObject toJSON() throws JSONException {
            final JSONObject json = new JSONObject();

            json.put(JSON_KEY_TYPE, mType.toString());
            json.put(JSON_KEY_TITLE, mTitle);
            json.put(JSON_KEY_ID, mId);

            if (mLayoutType != null) {
                json.put(JSON_KEY_LAYOUT, mLayoutType.toString());
            }

            if (mViews != null) {
                final JSONArray jsonViews = new JSONArray();

                final int viewCount = mViews.size();
                for (int i = 0; i < viewCount; i++) {
                    final ViewConfig viewConfig = mViews.get(i);
                    final JSONObject jsonViewConfig = viewConfig.toJSON();
                    jsonViews.put(jsonViewConfig);
                }

                json.put(JSON_KEY_VIEWS, jsonViews);
            }

            if (mAuthConfig != null) {
                json.put(JSON_KEY_AUTH_CONFIG, mAuthConfig.toJSON());
            }

            if (mFlags.contains(Flags.DEFAULT_PANEL)) {
                json.put(JSON_KEY_DEFAULT, true);
            }

            if (mFlags.contains(Flags.DISABLED_PANEL)) {
                json.put(JSON_KEY_DISABLED, true);
            }

            return json;
        }

        @Override
        public boolean equals(Object o) {
            if (o == null) {
                return false;
            }

            if (this == o) {
                return true;
            }

            if (!(o instanceof PanelConfig)) {
                return false;
            }

            final PanelConfig other = (PanelConfig) o;
            return mId.equals(other.mId);
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeParcelable(mType, 0);
            dest.writeString(mTitle);
            dest.writeString(mId);
            dest.writeParcelable(mLayoutType, 0);
            dest.writeTypedList(mViews);
            dest.writeParcelable(mAuthConfig, 0);
            dest.writeSerializable(mFlags);
        }

        public static final Creator<PanelConfig> CREATOR = new Creator<PanelConfig>() {
            @Override
            public PanelConfig createFromParcel(final Parcel in) {
                return new PanelConfig(in);
            }

            @Override
            public PanelConfig[] newArray(final int size) {
                return new PanelConfig[size];
            }
        };
    }

    public static enum LayoutType implements Parcelable {
        FRAME("frame");

        private final String mId;

        LayoutType(String id) {
            mId = id;
        }

        public static LayoutType fromId(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Could not convert null String to LayoutType");
            }

            for (LayoutType layoutType : LayoutType.values()) {
                if (TextUtils.equals(layoutType.mId, id.toLowerCase())) {
                    return layoutType;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to LayoutType");
        }

        @Override
        public String toString() {
            return mId;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<LayoutType> CREATOR = new Creator<LayoutType>() {
            @Override
            public LayoutType createFromParcel(final Parcel source) {
                return LayoutType.values()[source.readInt()];
            }

            @Override
            public LayoutType[] newArray(final int size) {
                return new LayoutType[size];
            }
        };
    }

    public static enum ViewType implements Parcelable {
        LIST("list"),
        GRID("grid");

        private final String mId;

        ViewType(String id) {
            mId = id;
        }

        public static ViewType fromId(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Could not convert null String to ViewType");
            }

            for (ViewType viewType : ViewType.values()) {
                if (TextUtils.equals(viewType.mId, id.toLowerCase())) {
                    return viewType;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to ViewType");
        }

        @Override
        public String toString() {
            return mId;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<ViewType> CREATOR = new Creator<ViewType>() {
            @Override
            public ViewType createFromParcel(final Parcel source) {
                return ViewType.values()[source.readInt()];
            }

            @Override
            public ViewType[] newArray(final int size) {
                return new ViewType[size];
            }
        };
    }

    public static enum ItemType implements Parcelable {
        ARTICLE("article"),
        IMAGE("image");

        private final String mId;

        ItemType(String id) {
            mId = id;
        }

        public static ItemType fromId(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Could not convert null String to ItemType");
            }

            for (ItemType itemType : ItemType.values()) {
                if (TextUtils.equals(itemType.mId, id.toLowerCase())) {
                    return itemType;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to ItemType");
        }

        @Override
        public String toString() {
            return mId;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<ItemType> CREATOR = new Creator<ItemType>() {
            @Override
            public ItemType createFromParcel(final Parcel source) {
                return ItemType.values()[source.readInt()];
            }

            @Override
            public ItemType[] newArray(final int size) {
                return new ItemType[size];
            }
        };
    }

    public static enum ItemHandler implements Parcelable {
        BROWSER("browser"),
        INTENT("intent");

        private final String mId;

        ItemHandler(String id) {
            mId = id;
        }

        public static ItemHandler fromId(String id) {
            if (id == null) {
                throw new IllegalArgumentException("Could not convert null String to ItemHandler");
            }

            for (ItemHandler itemHandler : ItemHandler.values()) {
                if (TextUtils.equals(itemHandler.mId, id.toLowerCase())) {
                    return itemHandler;
                }
            }

            throw new IllegalArgumentException("Could not convert String id to ItemHandler");
        }

        @Override
        public String toString() {
            return mId;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(ordinal());
        }

        public static final Creator<ItemHandler> CREATOR = new Creator<ItemHandler>() {
            @Override
            public ItemHandler createFromParcel(final Parcel source) {
                return ItemHandler.values()[source.readInt()];
            }

            @Override
            public ItemHandler[] newArray(final int size) {
                return new ItemHandler[size];
            }
        };
    }

    public static class ViewConfig implements Parcelable {
        private final int mIndex;
        private final ViewType mType;
        private final String mDatasetId;
        private final ItemType mItemType;
        private final ItemHandler mItemHandler;
        private final String mBackImageUrl;
        private final String mFilter;
        private final EmptyViewConfig mEmptyViewConfig;
        private final EnumSet<Flags> mFlags;

        private static final String JSON_KEY_TYPE = "type";
        private static final String JSON_KEY_DATASET = "dataset";
        private static final String JSON_KEY_ITEM_TYPE = "itemType";
        private static final String JSON_KEY_ITEM_HANDLER = "itemHandler";
        private static final String JSON_KEY_BACK_IMAGE_URL = "backImageUrl";
        private static final String JSON_KEY_FILTER = "filter";
        private static final String JSON_KEY_EMPTY = "empty";
        private static final String JSON_KEY_REFRESH_ENABLED = "refreshEnabled";

        public enum Flags {
            REFRESH_ENABLED
        }

        public ViewConfig(int index, JSONObject json) throws JSONException, IllegalArgumentException {
            mIndex = index;
            mType = ViewType.fromId(json.getString(JSON_KEY_TYPE));
            mDatasetId = json.getString(JSON_KEY_DATASET);
            mItemType = ItemType.fromId(json.getString(JSON_KEY_ITEM_TYPE));
            mItemHandler = ItemHandler.fromId(json.getString(JSON_KEY_ITEM_HANDLER));
            mBackImageUrl = json.optString(JSON_KEY_BACK_IMAGE_URL, null);
            mFilter = json.optString(JSON_KEY_FILTER, null);

            final JSONObject jsonEmptyViewConfig = json.optJSONObject(JSON_KEY_EMPTY);
            if (jsonEmptyViewConfig != null) {
                mEmptyViewConfig = new EmptyViewConfig(jsonEmptyViewConfig);
            } else {
                mEmptyViewConfig = null;
            }

            mFlags = EnumSet.noneOf(Flags.class);
            if (json.optBoolean(JSON_KEY_REFRESH_ENABLED, false)) {
                mFlags.add(Flags.REFRESH_ENABLED);
            }

            validate();
        }

        @SuppressWarnings("unchecked")
        public ViewConfig(Parcel in) {
            mIndex = in.readInt();
            mType = (ViewType) in.readParcelable(getClass().getClassLoader());
            mDatasetId = in.readString();
            mItemType = (ItemType) in.readParcelable(getClass().getClassLoader());
            mItemHandler = (ItemHandler) in.readParcelable(getClass().getClassLoader());
            mBackImageUrl = in.readString();
            mFilter = in.readString();
            mEmptyViewConfig = (EmptyViewConfig) in.readParcelable(getClass().getClassLoader());
            mFlags = (EnumSet<Flags>) in.readSerializable();

            validate();
        }

        public ViewConfig(ViewConfig viewConfig) {
            mIndex = viewConfig.mIndex;
            mType = viewConfig.mType;
            mDatasetId = viewConfig.mDatasetId;
            mItemType = viewConfig.mItemType;
            mItemHandler = viewConfig.mItemHandler;
            mBackImageUrl = viewConfig.mBackImageUrl;
            mFilter = viewConfig.mFilter;
            mEmptyViewConfig = viewConfig.mEmptyViewConfig;
            mFlags = viewConfig.mFlags.clone();

            validate();
        }

        public ViewConfig(int index, ViewType type, String datasetId, ItemType itemType,
                          ItemHandler itemHandler, String backImageUrl, String filter,
                          EmptyViewConfig emptyViewConfig, EnumSet<Flags> flags) {
            mIndex = index;
            mType = type;
            mDatasetId = datasetId;
            mItemType = itemType;
            mItemHandler = itemHandler;
            mBackImageUrl = backImageUrl;
            mFilter = filter;
            mEmptyViewConfig = emptyViewConfig;
            mFlags = flags;

            validate();
        }

        private void validate() {
            if (mType == null) {
                throw new IllegalArgumentException("Can't create ViewConfig with null type");
            }

            if (TextUtils.isEmpty(mDatasetId)) {
                throw new IllegalArgumentException("Can't create ViewConfig with empty dataset ID");
            }

            if (mItemType == null) {
                throw new IllegalArgumentException("Can't create ViewConfig with null item type");
            }

            if (mItemHandler == null) {
                throw new IllegalArgumentException("Can't create ViewConfig with null item handler");
            }

            if (mFlags == null) {
               throw new IllegalArgumentException("Can't create ViewConfig with null flags");
            }
        }

        public int getIndex() {
            return mIndex;
        }

        public ViewType getType() {
            return mType;
        }

        public String getDatasetId() {
            return mDatasetId;
        }

        public ItemType getItemType() {
            return mItemType;
        }

        public ItemHandler getItemHandler() {
            return mItemHandler;
        }

        public String getBackImageUrl() {
            return mBackImageUrl;
        }

        public String getFilter() {
            return mFilter;
        }

        public EmptyViewConfig getEmptyViewConfig() {
            return mEmptyViewConfig;
        }

        public boolean isRefreshEnabled() {
            return mFlags.contains(Flags.REFRESH_ENABLED);
        }

        public JSONObject toJSON() throws JSONException {
            final JSONObject json = new JSONObject();

            json.put(JSON_KEY_TYPE, mType.toString());
            json.put(JSON_KEY_DATASET, mDatasetId);
            json.put(JSON_KEY_ITEM_TYPE, mItemType.toString());
            json.put(JSON_KEY_ITEM_HANDLER, mItemHandler.toString());

            if (!TextUtils.isEmpty(mBackImageUrl)) {
                json.put(JSON_KEY_BACK_IMAGE_URL, mBackImageUrl);
            }

            if (!TextUtils.isEmpty(mFilter)) {
                json.put(JSON_KEY_FILTER, mFilter);
            }

            if (mEmptyViewConfig != null) {
                json.put(JSON_KEY_EMPTY, mEmptyViewConfig.toJSON());
            }

            if (mFlags.contains(Flags.REFRESH_ENABLED)) {
                json.put(JSON_KEY_REFRESH_ENABLED, true);
            }

            return json;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(mIndex);
            dest.writeParcelable(mType, 0);
            dest.writeString(mDatasetId);
            dest.writeParcelable(mItemType, 0);
            dest.writeParcelable(mItemHandler, 0);
            dest.writeString(mBackImageUrl);
            dest.writeString(mFilter);
            dest.writeParcelable(mEmptyViewConfig, 0);
            dest.writeSerializable(mFlags);
        }

        public static final Creator<ViewConfig> CREATOR = new Creator<ViewConfig>() {
            @Override
            public ViewConfig createFromParcel(final Parcel in) {
                return new ViewConfig(in);
            }

            @Override
            public ViewConfig[] newArray(final int size) {
                return new ViewConfig[size];
            }
        };
    }

    public static class EmptyViewConfig implements Parcelable {
        private final String mText;
        private final String mImageUrl;

        private static final String JSON_KEY_TEXT = "text";
        private static final String JSON_KEY_IMAGE_URL = "imageUrl";

        public EmptyViewConfig(JSONObject json) throws JSONException, IllegalArgumentException {
            mText = json.optString(JSON_KEY_TEXT, null);
            mImageUrl = json.optString(JSON_KEY_IMAGE_URL, null);
        }

        @SuppressWarnings("unchecked")
        public EmptyViewConfig(Parcel in) {
            mText = in.readString();
            mImageUrl = in.readString();
        }

        public EmptyViewConfig(EmptyViewConfig emptyViewConfig) {
            mText = emptyViewConfig.mText;
            mImageUrl = emptyViewConfig.mImageUrl;
        }

        public EmptyViewConfig(String text, String imageUrl) {
            mText = text;
            mImageUrl = imageUrl;
        }

        public String getText() {
            return mText;
        }

        public String getImageUrl() {
            return mImageUrl;
        }

        public JSONObject toJSON() throws JSONException {
            final JSONObject json = new JSONObject();

            json.put(JSON_KEY_TEXT, mText);
            json.put(JSON_KEY_IMAGE_URL, mImageUrl);

            return json;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeString(mText);
            dest.writeString(mImageUrl);
        }

        public static final Creator<EmptyViewConfig> CREATOR = new Creator<EmptyViewConfig>() {
            @Override
            public EmptyViewConfig createFromParcel(final Parcel in) {
                return new EmptyViewConfig(in);
            }

            @Override
            public EmptyViewConfig[] newArray(final int size) {
                return new EmptyViewConfig[size];
            }
        };
    }

    public static class AuthConfig implements Parcelable {
        private final String mMessageText;
        private final String mButtonText;
        private final String mImageUrl;

        private static final String JSON_KEY_MESSAGE_TEXT = "messageText";
        private static final String JSON_KEY_BUTTON_TEXT = "buttonText";
        private static final String JSON_KEY_IMAGE_URL = "imageUrl";

        public AuthConfig(JSONObject json) throws JSONException, IllegalArgumentException {
            mMessageText = json.optString(JSON_KEY_MESSAGE_TEXT);
            mButtonText = json.optString(JSON_KEY_BUTTON_TEXT);
            mImageUrl = json.optString(JSON_KEY_IMAGE_URL, null);
        }

        @SuppressWarnings("unchecked")
        public AuthConfig(Parcel in) {
            mMessageText = in.readString();
            mButtonText = in.readString();
            mImageUrl = in.readString();

            validate();
        }

        public AuthConfig(AuthConfig authConfig) {
            mMessageText = authConfig.mMessageText;
            mButtonText = authConfig.mButtonText;
            mImageUrl = authConfig.mImageUrl;

            validate();
        }

        public AuthConfig(String messageText, String buttonText, String imageUrl) {
            mMessageText = messageText;
            mButtonText = buttonText;
            mImageUrl = imageUrl;

            validate();
        }

        private void validate() {
            if (mMessageText == null) {
                throw new IllegalArgumentException("Can't create AuthConfig with null message text");
            }

            if (mButtonText == null) {
                throw new IllegalArgumentException("Can't create AuthConfig with null button text");
            }
        }

        public String getMessageText() {
            return mMessageText;
        }

        public String getButtonText() {
            return mButtonText;
        }

        public String getImageUrl() {
            return mImageUrl;
        }

        public JSONObject toJSON() throws JSONException {
            final JSONObject json = new JSONObject();

            json.put(JSON_KEY_MESSAGE_TEXT, mMessageText);
            json.put(JSON_KEY_BUTTON_TEXT, mButtonText);
            json.put(JSON_KEY_IMAGE_URL, mImageUrl);

            return json;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeString(mMessageText);
            dest.writeString(mButtonText);
            dest.writeString(mImageUrl);
        }

        public static final Creator<AuthConfig> CREATOR = new Creator<AuthConfig>() {
            @Override
            public AuthConfig createFromParcel(final Parcel in) {
                return new AuthConfig(in);
            }

            @Override
            public AuthConfig[] newArray(final int size) {
                return new AuthConfig[size];
            }
        };
    }
   /**
     * Immutable representation of the current state of {@code HomeConfig}.
     * This is what HomeConfig returns from a load() call and takes as
     * input to save a new state.
     *
     * Users of {@code State} should use an {@code Iterator} to iterate
     * through the contained {@code PanelConfig} instances.
     *
     * {@code State} is immutable i.e. you can't add, remove, or update
     * contained elements directly. You have to use an {@code Editor} to
     * change the state, which can be created through the {@code edit()}
     * method.
     */
    public static class State implements Iterable<PanelConfig> {
        private HomeConfig mHomeConfig;
        private final List<PanelConfig> mPanelConfigs;
        private final boolean mIsDefault;

        State(List<PanelConfig> panelConfigs, boolean isDefault) {
            this(null, panelConfigs, isDefault);
        }

        private State(HomeConfig homeConfig, List<PanelConfig> panelConfigs, boolean isDefault) {
            mHomeConfig = homeConfig;
            mPanelConfigs = Collections.unmodifiableList(panelConfigs);
            mIsDefault = isDefault;
        }

        private void setHomeConfig(HomeConfig homeConfig) {
            if (mHomeConfig != null) {
                throw new IllegalStateException("Can't set HomeConfig more than once");
            }

            mHomeConfig = homeConfig;
        }

        @Override
        public Iterator<PanelConfig> iterator() {
            return mPanelConfigs.iterator();
        }

        /**
         * Returns whether this {@code State} instance represents the default
         * {@code HomeConfig} configuration or not.
         */
        public boolean isDefault() {
            return mIsDefault;
        }

        /**
         * Creates an {@code Editor} for this state.
         */
        public Editor edit() {
            return new Editor(mHomeConfig, this);
        }
    }

    /**
     * {@code Editor} allows you to make changes to a {@code State}. You
     * can create {@code Editor} by calling {@code edit()} on the target
     * {@code State} instance.
     *
     * {@code Editor} works on a copy of the {@code State} that originated
     * it. This means that adding, removing, or updating panels in an
     * {@code Editor} will never change the {@code State} which you
     * created the {@code Editor} from. Calling {@code commit()} or
     * {@code apply()} will cause the new {@code State} instance to be
     * created and saved using the {@code HomeConfig} instance that
     * created the source {@code State}.
     *
     * {@code Editor} is *not* thread-safe. You can only make calls on it
     * from the thread where it was originally created. It will throw an
     * exception if you don't follow this invariant.
     */
    public static class Editor implements Iterable<PanelConfig> {
        private final HomeConfig mHomeConfig;
        private final Map<String, PanelConfig> mConfigMap;
        private final List<String> mConfigOrder;
        private final List<GeckoEvent> mEventQueue;
        private final Thread mOriginalThread;

        private PanelConfig mDefaultPanel;
        private int mEnabledCount;

        private boolean mHasChanged;
        private final boolean mIsFromDefault;

        private Editor(HomeConfig homeConfig, State configState) {
            mHomeConfig = homeConfig;
            mOriginalThread = Thread.currentThread();
            mConfigMap = new HashMap<String, PanelConfig>();
            mConfigOrder = new LinkedList<String>();
            mEventQueue = new LinkedList<GeckoEvent>();
            mEnabledCount = 0;

            mHasChanged = false;
            mIsFromDefault = configState.isDefault();

            initFromState(configState);
        }

        /**
         * Initialize the initial state of the editor from the given
         * {@sode State}. A HashMap is used to represent the list of
         * panels as it provides fast access, and a LinkedList is used to
         * keep track of order. We keep a reference to the default panel
         * and the number of enabled panels to avoid iterating through the
         * map every time we need those.
         *
         * @param configState The source State to load the editor from.
         */
        private void initFromState(State configState) {
            for (PanelConfig panelConfig : configState) {
                final PanelConfig panelCopy = new PanelConfig(panelConfig);

                if (!panelCopy.isDisabled()) {
                    mEnabledCount++;
                }

                if (panelCopy.isDefault()) {
                    if (mDefaultPanel == null) {
                        mDefaultPanel = panelCopy;
                    } else {
                        throw new IllegalStateException("Multiple default panels in HomeConfig state");
                    }
                }

                final String panelId = panelConfig.getId();
                mConfigOrder.add(panelId);
                mConfigMap.put(panelId, panelCopy);
            }

            // We should always have a defined default panel if there's
            // at least one enabled panel around.
            if (mEnabledCount > 0 && mDefaultPanel == null) {
                throw new IllegalStateException("Default panel in HomeConfig state is undefined");
            }
        }

        private PanelConfig getPanelOrThrow(String panelId) {
            final PanelConfig panelConfig = mConfigMap.get(panelId);
            if (panelConfig == null) {
                throw new IllegalStateException("Tried to access non-existing panel: " + panelId);
            }

            return panelConfig;
        }

        private boolean isCurrentDefaultPanel(PanelConfig panelConfig) {
            if (mDefaultPanel == null) {
                return false;
            }

            return mDefaultPanel.equals(panelConfig);
        }

        private void findNewDefault() {
            // Pick the first panel that is neither disabled nor currently
            // set as default.
            for (PanelConfig panelConfig : mConfigMap.values()) {
                if (!panelConfig.isDefault() && !panelConfig.isDisabled()) {
                    setDefault(panelConfig.getId());
                    return;
                }
            }

            mDefaultPanel = null;
        }

        /**
         * Makes an ordered list of PanelConfigs that can be references
         * or deep copied objects.
         *
         * @param deepCopy true to make deep-copied objects
         * @return ordered List of PanelConfigs
         */
        private List<PanelConfig> makeOrderedCopy(boolean deepCopy) {
            final List<PanelConfig> copiedList = new ArrayList<PanelConfig>(mConfigOrder.size());
            for (String panelId : mConfigOrder) {
                PanelConfig panelConfig = mConfigMap.get(panelId);
                if (deepCopy) {
                    panelConfig = new PanelConfig(panelConfig);
                }
                copiedList.add(panelConfig);
            }

            return copiedList;
        }

        private void setPanelIsDisabled(PanelConfig panelConfig, boolean disabled) {
            if (panelConfig.isDisabled() == disabled) {
                return;
            }

            panelConfig.setIsDisabled(disabled);
            mEnabledCount += (disabled ? -1 : 1);
        }

        /**
         * Gets the ID of the current default panel.
         */
        public String getDefaultPanelId() {
            ThreadUtils.assertOnThread(mOriginalThread);

            if (mDefaultPanel == null) {
                return null;
            }

            return mDefaultPanel.getId();
        }

        /**
         * Set a new default panel.
         *
         * @param panelId the ID of the new default panel.
         */
        public void setDefault(String panelId) {
            ThreadUtils.assertOnThread(mOriginalThread);

            final PanelConfig panelConfig = getPanelOrThrow(panelId);
            if (isCurrentDefaultPanel(panelConfig)) {
                return;
            }

            if (mDefaultPanel != null) {
                mDefaultPanel.setIsDefault(false);
            }

            panelConfig.setIsDefault(true);
            setPanelIsDisabled(panelConfig, false);

            mDefaultPanel = panelConfig;
            mHasChanged = true;
        }

        /**
         * Toggles disabled state for a panel.
         *
         * @param panelId the ID of the target panel.
         * @param disabled true to disable the panel.
         */
        public void setDisabled(String panelId, boolean disabled) {
            ThreadUtils.assertOnThread(mOriginalThread);

            final PanelConfig panelConfig = getPanelOrThrow(panelId);
            if (panelConfig.isDisabled() == disabled) {
                return;
            }

            setPanelIsDisabled(panelConfig, disabled);

            if (disabled) {
                if (isCurrentDefaultPanel(panelConfig)) {
                    panelConfig.setIsDefault(false);
                    findNewDefault();
                }
            } else if (mEnabledCount == 1) {
                setDefault(panelId);
            }

            mHasChanged = true;
        }

        /**
         * Adds a new {@code PanelConfig}. It will do nothing if the
         * {@code Editor} already contains a panel with the same ID.
         *
         * @param panelConfig the {@code PanelConfig} instance to be added.
         * @return true if the item has been added.
         */
        public boolean install(PanelConfig panelConfig) {
            ThreadUtils.assertOnThread(mOriginalThread);

            if (panelConfig == null) {
                throw new IllegalStateException("Can't install a null panel");
            }

            if (!panelConfig.isDynamic()) {
                throw new IllegalStateException("Can't install a built-in panel: " + panelConfig.getId());
            }

            if (panelConfig.isDisabled()) {
                throw new IllegalStateException("Can't install a disabled panel: " + panelConfig.getId());
            }

            boolean installed = false;

            final String id = panelConfig.getId();
            if (!mConfigMap.containsKey(id)) {
                mConfigMap.put(id, panelConfig);
                mConfigOrder.add(id);

                mEnabledCount++;
                if (mEnabledCount == 1 || panelConfig.isDefault()) {
                    setDefault(panelConfig.getId());
                }

                installed = true;

                // Add an event to the queue if a new panel is sucessfully installed.
                mEventQueue.add(GeckoEvent.createBroadcastEvent("HomePanels:Installed", panelConfig.getId()));
            }

            mHasChanged = true;
            return installed;
        }

        /**
         * Removes an existing panel.
         *
         * @return true if the item has been removed.
         */
        public boolean uninstall(String panelId) {
            ThreadUtils.assertOnThread(mOriginalThread);

            final PanelConfig panelConfig = mConfigMap.get(panelId);
            if (panelConfig == null) {
                return false;
            }

            if (!panelConfig.isDynamic()) {
                throw new IllegalStateException("Can't uninstall a built-in panel: " + panelConfig.getId());
            }

            mConfigMap.remove(panelId);
            mConfigOrder.remove(panelId);

            if (!panelConfig.isDisabled()) {
                mEnabledCount--;
            }

            if (isCurrentDefaultPanel(panelConfig)) {
                findNewDefault();
            }

            // Add an event to the queue if a panel is succesfully uninstalled.
            mEventQueue.add(GeckoEvent.createBroadcastEvent("HomePanels:Uninstalled", panelId));

            mHasChanged = true;
            return true;
        }

        /**
         * Moves panel associated with panelId to the specified position.
         *
         * @param panelId Id of panel
         * @param destIndex Destination position
         * @return true if move succeeded
         */
        public boolean moveTo(String panelId, int destIndex) {
            ThreadUtils.assertOnThread(mOriginalThread);

            if (!mConfigOrder.contains(panelId)) {
                return false;
            }

            mConfigOrder.remove(panelId);
            mConfigOrder.add(destIndex, panelId);
            mHasChanged = true;

            return true;
        }

        /**
         * Replaces an existing panel with a new {@code PanelConfig} instance.
         *
         * @return true if the item has been updated.
         */
        public boolean update(PanelConfig panelConfig) {
            ThreadUtils.assertOnThread(mOriginalThread);

            if (panelConfig == null) {
                throw new IllegalStateException("Can't update a null panel");
            }

            boolean updated = false;

            final String id = panelConfig.getId();
            if (mConfigMap.containsKey(id)) {
                final PanelConfig oldPanelConfig = mConfigMap.put(id, panelConfig);

                // The disabled and default states can't never be
                // changed by an update operation.
                panelConfig.setIsDefault(oldPanelConfig.isDefault());
                panelConfig.setIsDisabled(oldPanelConfig.isDisabled());

                updated = true;
            }

            mHasChanged = true;
            return updated;
        }

        /**
         * Saves the current {@code Editor} state asynchronously in the
         * background thread.
         *
         * @return the resulting {@code State} instance.
         */
        public State apply() {
            ThreadUtils.assertOnThread(mOriginalThread);

            // We're about to save the current state in the background thread
            // so we should use a deep copy of the PanelConfig instances to
            // avoid saving corrupted state.
            final State newConfigState =
                    new State(mHomeConfig, makeOrderedCopy(true), isDefault());

            // Copy the event queue to a new list, so that we only modify mEventQueue on
            // the original thread where it was created.
            final LinkedList<GeckoEvent> eventQueueCopy = new LinkedList<GeckoEvent>(mEventQueue);
            mEventQueue.clear();

            ThreadUtils.getBackgroundHandler().post(new Runnable() {
                @Override
                public void run() {
                    mHomeConfig.save(newConfigState);

                    // Send pending events after the new config is saved.
                    sendEventsToGecko(eventQueueCopy);
                }
            });

            return newConfigState;
        }

        /**
         * Saves the current {@code Editor} state synchronously in the
         * current thread.
         *
         * @return the resulting {@code State} instance.
         */
        public State commit() {
            ThreadUtils.assertOnThread(mOriginalThread);

            final State newConfigState =
                    new State(mHomeConfig, makeOrderedCopy(false), isDefault());

            // This is a synchronous blocking operation, hence no
            // need to deep copy the current PanelConfig instances.
            mHomeConfig.save(newConfigState);

            // Send pending events after the new config is saved.
            sendEventsToGecko(mEventQueue);
            mEventQueue.clear();

            return newConfigState;
        }

        /**
         * Returns whether the {@code Editor} represents the default
         * {@code HomeConfig} configuration without any unsaved changes.
         */
        public boolean isDefault() {
            ThreadUtils.assertOnThread(mOriginalThread);

            return (!mHasChanged && mIsFromDefault);
        }

        public boolean isEmpty() {
            return mConfigMap.isEmpty();
        }

        private void sendEventsToGecko(List<GeckoEvent> events) {
            for (GeckoEvent e : events) {
                GeckoAppShell.sendEventToGecko(e);
            }
        }

        private class EditorIterator implements Iterator<PanelConfig> {
            private final Iterator<String> mOrderIterator;

            public EditorIterator() {
                mOrderIterator = mConfigOrder.iterator();
            }

            @Override
            public boolean hasNext() {
                return mOrderIterator.hasNext();
            }

            @Override
            public PanelConfig next() {
                final String panelId = mOrderIterator.next();
                return mConfigMap.get(panelId);
            }

            @Override
            public void remove() {
                throw new UnsupportedOperationException("Can't 'remove' from on Editor iterator.");
            }
        }

        @Override
        public Iterator<PanelConfig> iterator() {
            ThreadUtils.assertOnThread(mOriginalThread);

            return new EditorIterator();
        }
    }

    public interface OnReloadListener {
        public void onReload();
    }

    public interface HomeConfigBackend {
        public State load();
        public void save(State configState);
        public String getLocale();
        public void setOnReloadListener(OnReloadListener listener);
    }

    // UUIDs used to create PanelConfigs for default built-in panels
    private static final String TOP_SITES_PANEL_ID = "4becc86b-41eb-429a-a042-88fe8b5a094e";
    private static final String BOOKMARKS_PANEL_ID = "7f6d419a-cd6c-4e34-b26f-f68b1b551907";
    private static final String READING_LIST_PANEL_ID = "20f4549a-64ad-4c32-93e4-1dcef792733b";
    private static final String HISTORY_PANEL_ID = "f134bf20-11f7-4867-ab8b-e8e705d7fbe8";
    private static final String RECENT_TABS_PANEL_ID = "5c2601a5-eedc-4477-b297-ce4cef52adf8";

    private final HomeConfigBackend mBackend;

    public HomeConfig(HomeConfigBackend backend) {
        mBackend = backend;
    }

    public State load() {
        final State configState = mBackend.load();
        configState.setHomeConfig(this);

        return configState;
    }

    public String getLocale() {
        return mBackend.getLocale();
    }

    public void save(State configState) {
        mBackend.save(configState);
    }

    public void setOnReloadListener(OnReloadListener listener) {
        mBackend.setOnReloadListener(listener);
    }

    public static PanelConfig createBuiltinPanelConfig(Context context, PanelType panelType) {
        return createBuiltinPanelConfig(context, panelType, EnumSet.noneOf(PanelConfig.Flags.class));
    }

    public static PanelConfig createBuiltinPanelConfig(Context context, PanelType panelType, EnumSet<PanelConfig.Flags> flags) {
        int titleId = 0;
        String id = null;

        switch(panelType) {
            case TOP_SITES:
                titleId = R.string.home_top_sites_title;
                id = TOP_SITES_PANEL_ID;
                break;

            case BOOKMARKS:
                titleId = R.string.bookmarks_title;
                id = BOOKMARKS_PANEL_ID;
                break;

            case HISTORY:
                titleId = R.string.home_history_title;
                id = HISTORY_PANEL_ID;
                break;

            case READING_LIST:
                titleId = R.string.reading_list_title;
                id = READING_LIST_PANEL_ID;
                break;

            case RECENT_TABS:
                titleId = R.string.recent_tabs_title;
                id = RECENT_TABS_PANEL_ID;
                break;

            case DYNAMIC:
                throw new IllegalArgumentException("createBuiltinPanelConfig() is only for built-in panels");
        }

        return new PanelConfig(panelType, context.getString(titleId), id, flags);
    }

    public static HomeConfig getDefault(Context context) {
        return new HomeConfig(new HomeConfigPrefsBackend(context));
    }
}
