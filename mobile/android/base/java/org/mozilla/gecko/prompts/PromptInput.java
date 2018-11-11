/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.prompts;

import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.GregorianCalendar;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.widget.AllCapsTextView;
import org.mozilla.gecko.widget.FocusableDatePicker;
import org.mozilla.gecko.widget.DateTimePicker;
import org.mozilla.gecko.widget.FocusableTimePicker;

import android.content.Context;
import android.content.res.Configuration;
import android.support.annotation.NonNull;
import android.support.design.widget.TextInputLayout;
import android.text.Html;
import android.text.InputType;
import android.text.TextUtils;
import android.text.format.DateFormat;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.DatePicker;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.TimePicker;

public abstract class PromptInput {
    protected final String mLabel;
    protected final String mType;
    protected final String mId;
    protected String mValue;
    protected final String mMinValue;
    protected final String mMaxValue;
    protected final boolean mSecondEnabled;
    protected OnChangeListener mListener;
    protected View mView;
    public static final String LOGTAG = "GeckoPromptInput";

    public interface OnChangeListener {
        void onChange(PromptInput input);
    }

    public void setListener(OnChangeListener listener) {
        mListener = listener;
    }

    public static class EditInput extends PromptInput {
        protected final String mHint;
        protected final boolean mAutofocus;
        public static final String INPUT_TYPE = "textbox";

        public EditInput(GeckoBundle object) {
            super(object);
            mHint = object.getString("hint", "");
            mAutofocus = object.getBoolean("autofocus");
        }

        @Override
        public View getView(final Context context) throws UnsupportedOperationException {
            EditText input = new EditText(context);
            input.setInputType(InputType.TYPE_CLASS_TEXT);
            input.setText(mValue);

            if (!TextUtils.isEmpty(mHint)) {
                input.setHint(mHint);
            }

            if (mAutofocus) {
                input.setOnFocusChangeListener(new View.OnFocusChangeListener() {
                    @Override
                    public void onFocusChange(View v, boolean hasFocus) {
                        if (hasFocus) {
                            ((InputMethodManager) context.getSystemService(Context.INPUT_METHOD_SERVICE)).showSoftInput(v, 0);
                        }
                    }
                });
                input.requestFocus();
            }

            TextInputLayout inputLayout = new TextInputLayout(context);
            inputLayout.addView(input);

            mView = (View) inputLayout;
            return mView;
        }

        @Override
        public Object getValue() {
            final TextInputLayout inputLayout = (TextInputLayout) mView;
            return inputLayout.getEditText().getText();
        }
    }

    public static class NumberInput extends EditInput {
        public static final String INPUT_TYPE = "number";
        public NumberInput(GeckoBundle obj) {
            super(obj);
        }

        @Override
        public View getView(final Context context) throws UnsupportedOperationException {
            final TextInputLayout inputLayout = (TextInputLayout) super.getView(context);
            final EditText input = inputLayout.getEditText();
            input.setRawInputType(Configuration.KEYBOARD_12KEY);
            input.setInputType(InputType.TYPE_CLASS_NUMBER |
                               InputType.TYPE_NUMBER_FLAG_SIGNED);
            return inputLayout;
        }
    }

    public static class PasswordInput extends EditInput {
        public static final String INPUT_TYPE = "password";
        public PasswordInput(GeckoBundle obj) {
            super(obj);
        }

        @Override
        public View getView(Context context) throws UnsupportedOperationException {
            final TextInputLayout inputLayout = (TextInputLayout) super.getView(context);
            inputLayout.getEditText().setInputType(InputType.TYPE_CLASS_TEXT |
                               InputType.TYPE_TEXT_VARIATION_PASSWORD |
                               InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
            return inputLayout;
        }
    }

    public static class CheckboxInput extends PromptInput {
        public static final String INPUT_TYPE = "checkbox";
        private boolean mChecked;

        public CheckboxInput(GeckoBundle obj) {
            super(obj);
            mChecked = obj.getBoolean("checked");
        }

        @Override
        public View getView(Context context) throws UnsupportedOperationException {
            final CheckBox checkbox = new CheckBox(context);
            checkbox.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
            checkbox.setText(mLabel);
            checkbox.setChecked(mChecked);
            mView = (View)checkbox;
            return mView;
        }

        @Override
        public Object getValue() {
            CheckBox checkbox = (CheckBox)mView;
            return checkbox.isChecked() ? Boolean.TRUE : Boolean.FALSE;
        }

        @Override
        public void saveCurrentInput(@NonNull final GeckoBundle userInput) {
            if (userInput.containsKey(mId)) {
                mChecked = (Boolean) userInput.get(mId);
            }
        }
    }

    public static class DateTimeInput extends PromptInput {
        public static final String[] INPUT_TYPES = new String[] {
            "date",
            "week",
            "time",
            "datetime-local",
            "month"
        };

        public DateTimeInput(GeckoBundle obj) {
            super(obj);
        }

        // Will use platform's DatePicker and TimePicker to let users input date and time using the fancy widgets.
        // For the other input types our custom DateTimePicker will offer spinners.
        @Override
        public View getView(Context context) throws UnsupportedOperationException {
            if (mType.equals("date")) {
                // FocusableDatePicker allow us to have priority in responding to scroll events.
                DatePicker input = new FocusableDatePicker(context);
                try {
                    if (!TextUtils.isEmpty(mValue)) {
                        GregorianCalendar calendar = new GregorianCalendar();
                        calendar.setTime(new SimpleDateFormat("yyyy-MM-dd").parse(mValue));
                        input.updateDate(calendar.get(Calendar.YEAR),
                                         calendar.get(Calendar.MONTH),
                                         calendar.get(Calendar.DAY_OF_MONTH));
                    }
                } catch (Exception e) {
                    Log.e(LOGTAG, "error parsing format string: " + e);
                }

                // The Material CalendarView is only available on Android >= API 21
                // Prior to this, using DatePicker with CalendarUI would cause issues
                // such as in Bug 1460585 and Bug 1462299
                // Because of this, on Android versions earlier than API 21 we'll force use the SpinnerUI
                if (AppConstants.Versions.preLollipop) {
                    input.setSpinnersShown(true);
                    input.setCalendarViewShown(false);
                }

                mView = (View)input;
            } else if (mType.equals("week")) {
                DateTimePicker input = new DateTimePicker(context, "yyyy-'W'ww", mValue,
                                                          DateTimePicker.PickersState.WEEK, mMinValue, mMaxValue);
                mView = (View)input;
            } else if (mType.equals("time") && mSecondEnabled) {
                // When seconds are requested, use DateTimePicker since FocusableDatePicker does not support seconds.
                DateTimePicker input = new DateTimePicker(context, "HH:mm:ss",
                                                          formatDateTimeSeconds(mValue),
                                                          DateTimePicker.PickersState.TIME,
                                                          formatDateTimeSeconds(mMinValue),
                                                          formatDateTimeSeconds(mMaxValue));
                mView = (View)input;
            } else if (mType.equals("time")) {
                // FocusableDatePicker allow us to have priority in responding to scroll events.
                TimePicker input = new FocusableTimePicker(context);
                input.setIs24HourView(DateFormat.is24HourFormat(context));

                GregorianCalendar calendar = new GregorianCalendar();
                if (!TextUtils.isEmpty(mValue)) {
                    try {
                        calendar.setTime(new SimpleDateFormat("HH:mm").parse(mValue));
                    } catch (Exception e) { }
                }
                input.setCurrentHour(calendar.get(GregorianCalendar.HOUR_OF_DAY));
                input.setCurrentMinute(calendar.get(GregorianCalendar.MINUTE));
                mView = (View)input;
            } else if (mType.equals("datetime-local")) {
                DateTimePicker input = new DateTimePicker(context, "yyyy-MM-dd'T'HH:mm:ss",
                                                          formatDateTimeSeconds(mValue),
                                                          mSecondEnabled ? DateTimePicker.PickersState.DATETIME_WITH_SECOND : DateTimePicker.PickersState.DATETIME,
                                                          formatDateTimeSeconds(mMinValue),
                                                          formatDateTimeSeconds(mMaxValue));
                mView = (View)input;
            } else if (mType.equals("month")) {
                DateTimePicker input = new DateTimePicker(context, "yyyy-MM", mValue,
                                                          DateTimePicker.PickersState.MONTH, mMinValue, mMaxValue);
                mView = (View)input;
            }

            // Make sure the widgets will not be chopped on smaller screens (Bug 1412517)
            LinearLayout.LayoutParams parentParams = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            parentParams.gravity = Gravity.CENTER;
            mView.setLayoutParams(parentParams);

            return mView;
        }

        private static String formatDateTimeSeconds(String dateString) {
            // Reformat the datetime value so that it can be parsed by
            // SimpleDateFormat ending with "HH:mm:ss".

            int i = dateString.indexOf(":"); // Separator in "HH:mm".
            if (i == -1) {
                // Unparseable input.
                return dateString;
            }

            i = dateString.indexOf(":", i + 1); // Separator in "mm:ss".
            if (i == -1) {
                // Append seconds.
                return dateString + ":00";
            }

            return dateString;
        }

        private static String formatDateString(String dateFormat, Calendar calendar) {
            return new SimpleDateFormat(dateFormat).format(calendar.getTime());
        }

        @Override
        public Object getValue() {
            if (mType.equals("time")) {
                if (mSecondEnabled) {
                    DateTimePicker dp = (DateTimePicker) mView;
                    GregorianCalendar calendar = new GregorianCalendar();
                    calendar.setTimeInMillis(dp.getTimeInMillis());
                    return formatDateString("HH:mm:ss", calendar);
                }
                TimePicker tp = (TimePicker)mView;
                GregorianCalendar calendar =
                    new GregorianCalendar(0, 0, 0, tp.getCurrentHour(), tp.getCurrentMinute());
                return formatDateString("HH:mm", calendar);
            } else if (mType.equals("date")) {
                DatePicker dp = (DatePicker) mView;
                GregorianCalendar calendar =
                        new GregorianCalendar(dp.getYear(), dp.getMonth(), dp.getDayOfMonth());
                return formatDateString("yyyy-MM-dd", calendar);
            }
            else {
                DateTimePicker dp = (DateTimePicker) mView;
                GregorianCalendar calendar = new GregorianCalendar();
                calendar.setTimeInMillis(dp.getTimeInMillis());
                if (mType.equals("week")) {
                    return formatDateString("yyyy-'W'ww", calendar);
                } else if (mType.equals("datetime-local")) {
                    if (mSecondEnabled) {
                        return formatDateString("yyyy-MM-dd'T'HH:mm:ss", calendar);
                    }
                    return formatDateString("yyyy-MM-dd'T'HH:mm", calendar);
                } else if (mType.equals("month")) {
                    return formatDateString("yyyy-MM", calendar);
                }
            }
            return super.getValue();
        }
    }

    public static class MenulistInput extends PromptInput {
        public static final String INPUT_TYPE = "menulist";
        private final String[] mListitems;
        private int mSelected;

        public Spinner spinner;
        public AllCapsTextView textView;

        public MenulistInput(GeckoBundle obj) {
            super(obj);
            final String[] listitems = obj.getStringArray("values");
            mListitems = listitems != null ? listitems : new String[0];
            mSelected = obj.getInt("selected");
        }

        @Override
        public View getView(final Context context) throws UnsupportedOperationException {
            spinner = new Spinner(context, Spinner.MODE_DIALOG);
            try {
                if (mListitems.length > 0) {
                    ArrayAdapter<String> adapter = new ArrayAdapter<String>(context, android.R.layout.simple_spinner_item, mListitems);
                    adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

                    spinner.setAdapter(adapter);
                    spinner.setSelection(mSelected);
                }
            } catch (Exception ex) {
            }

            if (!TextUtils.isEmpty(mLabel)) {
                LinearLayout container = new LinearLayout(context);
                container.setOrientation(LinearLayout.VERTICAL);

                textView = new AllCapsTextView(context, null);
                textView.setText(mLabel);
                container.addView(textView);

                container.addView(spinner);
                return container;
            }

            return spinner;
        }

        @Override
        public Object getValue() {
            return spinner.getSelectedItemPosition();
        }

        @Override
        public void saveCurrentInput(@NonNull final GeckoBundle userInput) {
            if (userInput.containsKey(mId)) {
                mSelected = (Integer) userInput.get(mId);
            }
        }
    }

    public static class LabelInput extends PromptInput {
        public static final String INPUT_TYPE = "label";
        public LabelInput(GeckoBundle obj) {
            super(obj);
        }

        @Override
        public View getView(Context context) throws UnsupportedOperationException {
            // not really an input, but a way to add labels and such to the dialog
            TextView view = new TextView(context);
            view.setText(Html.fromHtml(mLabel));
            mView = view;
            return mView;
        }

        @Override
        public void saveCurrentInput(@NonNull final GeckoBundle userInput) {
            // No user input to save
        }
    }

    public PromptInput(GeckoBundle obj) {
        mLabel = obj.getString("label", "");
        mType = obj.getString("type", "");
        String id = obj.getString("id", "");
        mId = TextUtils.isEmpty(id) ? mType : id;
        mValue = obj.getString("value", "");
        mMaxValue = obj.getString("max", "");
        mMinValue = obj.getString("min", "");

        long timeStepInMs = obj.getLong("step", 0);
        mSecondEnabled = (timeStepInMs % 60000) != 0;
    }

    public void saveCurrentInput(@NonNull final GeckoBundle userInput) {
        if (userInput.containsKey(mId)) {
            mValue = (String) userInput.get(mId);
        }
    }

    public void putInBundle(final GeckoBundle bundle) {
        final String id = getId();
        final Object value = getValue();

        if (value == null) {
            bundle.putBundle(id, null);
        } else if (value instanceof Boolean) {
            bundle.putBoolean(id, (Boolean) value);
        } else if (value instanceof Double) {
            bundle.putDouble(id, (Double) value);
        } else if (value instanceof Integer) {
            bundle.putInt(id, (Integer) value);
        } else if (value instanceof CharSequence) {
            bundle.putString(id, value.toString());
        } else if (value instanceof GeckoBundle) {
            bundle.putBundle(id, (GeckoBundle) value);
        } else {
            throw new UnsupportedOperationException(value.getClass().toString());
        }
    }

    public static PromptInput getInput(GeckoBundle obj) {
        String type = obj.getString("type", "");
        switch (type) {
            case EditInput.INPUT_TYPE:
                return new EditInput(obj);
            case NumberInput.INPUT_TYPE:
                return new NumberInput(obj);
            case PasswordInput.INPUT_TYPE:
                return new PasswordInput(obj);
            case CheckboxInput.INPUT_TYPE:
                return new CheckboxInput(obj);
            case MenulistInput.INPUT_TYPE:
                return new MenulistInput(obj);
            case LabelInput.INPUT_TYPE:
                return new LabelInput(obj);
            case IconGridInput.INPUT_TYPE:
                return new IconGridInput(obj);
            case ColorPickerInput.INPUT_TYPE:
                return new ColorPickerInput(obj);
            case TabInput.INPUT_TYPE:
                return new TabInput(obj);
            default:
                for (String dtType : DateTimeInput.INPUT_TYPES) {
                    if (dtType.equals(type)) {
                        return new DateTimeInput(obj);
                    }
                }

                break;
        }

        return null;
    }

    public abstract View getView(Context context) throws UnsupportedOperationException;

    public String getId() {
        return mId;
    }

    public Object getValue() {
        return null;
    }

    public boolean getScrollable() {
        return false;
    }

    public boolean canApplyInputStyle() {
        return true;
    }

    protected void notifyListeners(String val) {
        if (mListener != null) {
            mListener.onChange(this);
        }
    }
}
