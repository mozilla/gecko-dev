/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.os.Parcel;
import android.os.Parcelable;
import androidx.annotation.NonNull;
import java.util.Objects;
import org.mozilla.gecko.annotation.WrapForJNI;

/**
 * This class corresponds to nsIAlertAction in Gecko, which again largely corresponds to each member
 * of <a href="https://developer.mozilla.org/en-US/docs/Web/API/Notification/actions">
 * Notification.actions</a>. It's passed to {@link WebNotification} and can be retrieved from it.
 */
public class WebNotificationAction implements Parcelable {
  /** The name of the notification action. */
  public final @NonNull String name;

  /** The title of the notification action. */
  public final @NonNull String title;

  @Override
  public int describeContents() {
    return 0;
  }

  @Override
  public void writeToParcel(final @NonNull Parcel parcel, final int i) {
    parcel.writeString(this.name);
    parcel.writeString(this.title);
  }

  /**
   * Constructs a WebNotificationAction with the specified name and title.
   *
   * @param name The name of the notification action.
   * @param title The title of the notification action.
   */
  @WrapForJNI
  public WebNotificationAction(final @NonNull String name, final @NonNull String title) {
    this.name = name;
    this.title = title;
  }

  private WebNotificationAction(final Parcel in) {
    this(Objects.requireNonNull(in.readString()), Objects.requireNonNull(in.readString()));
  }

  public static final Creator<WebNotificationAction> CREATOR =
      new Creator<WebNotificationAction>() {
        @Override
        public WebNotificationAction createFromParcel(final Parcel in) {
          return new WebNotificationAction(in);
        }

        @Override
        public WebNotificationAction[] newArray(final int size) {
          return new WebNotificationAction[size];
        }
      };
}
