/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.prompts.login

import android.content.Context
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.Divider
import androidx.compose.material.Icon
import androidx.compose.material.MaterialTheme
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalSoftwareKeyboardController
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.withStyledAttributes
import mozilla.components.compose.base.annotation.LightDarkPreview
import mozilla.components.concept.storage.Login
import mozilla.components.feature.prompts.R
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.ui.icons.R as iconsR

private val Login.passwordDisplay: String
    get() = "•".repeat(password.length)

private val Context.primrayColor: Color
    get() = Color(getColorFromAttr(android.R.attr.textColorPrimary))

private val Context.headerColor: Color
    get() {
        var color = Color(getColorFromAttr(android.R.attr.colorEdgeEffect))

        // Bug 1820554 - Until we figure out a good story for theming in a-c
        // using compose, we'll have to bridge to the old xml styled attributes
        // to get the colors we're inheriting from Fenix.
        withStyledAttributes(null, R.styleable.LoginSelectBar) {
            val resId = getResourceId(R.styleable.LoginSelectBar_mozacLoginSelectHeaderTextStyle, 0)
            if (resId > 0) {
                withStyledAttributes(resId, intArrayOf(android.R.attr.textColor)) {
                    color = Color(getColor(0, android.graphics.Color.MAGENTA))
                }
            }
        }

        return color
    }

/**
 * Colors used to theme [LoginPicker].
 *
 * @param primary The color used for the text in [LoginPicker].
 * @param header The color used for the header in [LoginPicker].
 */
data class LoginPickerColors(
    val primary: Color,
    val header: Color,
) {
    constructor(context: Context) : this(
        primary = context.primrayColor,
        header = context.headerColor,
    )
}

/**
 * Renders a single [Login] item.
 *
 * @param login The [Login] to be displayed.
 * @param loginPickerColors Colors used to style the text.
 * @param onListItemClicked Callback when a list item is clicked.
 */
@Composable
private fun LoginListItem(
    login: Login,
    loginPickerColors: LoginPickerColors,
    onListItemClicked: () -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 64.dp, top = 8.dp, end = 8.dp, bottom = 8.dp)
            .clickable { onListItemClicked() },
    ) {
        Text(
            text = login.username,
            color = loginPickerColors.primary,
            style = MaterialTheme.typography.body1,
        )

        Text(
            text = login.passwordDisplay,
            color = loginPickerColors.primary,
            style = MaterialTheme.typography.body2,
        )
    }
}

/**
 * Renders the header for the Login picker.
 *
 * @param isExpanded Whether or not the [LoginPicker] is expanded.
 * @param loginPickerColors Colors used to style the text.
 * @param modifier The [Modifier] used on this View.
 */
@Suppress("MagicNumber")
@Composable
private fun LoginPickerHeader(
    isExpanded: Boolean,
    loginPickerColors: LoginPickerColors,
    modifier: Modifier = Modifier,
) {
    val headerContentDescription = if (isExpanded) {
        stringResource(id = R.string.mozac_feature_prompts_collapse_logins_content_description_2)
    } else {
        stringResource(id = R.string.mozac_feature_prompts_expand_logins_content_description_2)
    }

    val chevronResourceId = if (isExpanded) {
        iconsR.drawable.mozac_ic_chevron_up_24
    } else {
        iconsR.drawable.mozac_ic_chevron_down_24
    }

    Row(
        modifier = modifier
            .semantics {
                contentDescription = headerContentDescription
            }
            .defaultMinSize(minHeight = 48.dp)
            .padding(horizontal = 16.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            painter = painterResource(id = iconsR.drawable.mozac_ic_login_24),
            contentDescription = null,
            tint = loginPickerColors.header,
        )

        Spacer(Modifier.width(24.dp))

        Text(
            text = stringResource(id = R.string.mozac_feature_prompts_saved_logins_2),
            color = loginPickerColors.header,
            fontSize = 16.sp,
            style = MaterialTheme.typography.subtitle2,
            modifier = Modifier.weight(1f),
        )

        Icon(
            painter = painterResource(id = chevronResourceId),
            contentDescription = null,
            tint = loginPickerColors.header,

        )
    }
}

/**
 * Renders the footer for the Login picker
 *
 * @param loginPickerColors Colors used to style the text.
 * @param modifier The [Modifier] used on this View.
 */
@Composable
private fun LoginPickerFooter(
    loginPickerColors: LoginPickerColors,
    modifier: Modifier = Modifier,
) {
    Row(
        modifier = modifier
            .padding(horizontal = 16.dp)
            .height(48.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Icon(
            painter = painterResource(id = iconsR.drawable.mozac_ic_settings_24),
            tint = loginPickerColors.primary,
            contentDescription = null,
        )

        Spacer(Modifier.width(24.dp))

        Text(
            text = stringResource(id = R.string.mozac_feature_prompts_manage_logins_2),
            color = loginPickerColors.primary,
            style = MaterialTheme.typography.subtitle2,
        )

        Spacer(Modifier.fillMaxWidth())
    }
}

/**
 * A list of [Login] items to autofill.
 *
 * @param logins A list of [Login]s to display.
 * @param isExpanded Whether or not the [LoginPicker] is expanded.
 * @param onExpandToggleClick Setter for [isExpanded].
 * @param onLoginSelected Callback when a login list item is tapped.
 * @param onManagePasswordClicked Callback when Manage Passwords is clicked.
 * @param loginPickerColors Colors used to style this component.
 */
@Composable
fun LoginPicker(
    logins: List<Login>,
    isExpanded: Boolean,
    onExpandToggleClick: (Boolean) -> Unit,
    onLoginSelected: (Login) -> Unit,
    onManagePasswordClicked: () -> Unit,
    loginPickerColors: LoginPickerColors,
) {
    val keyboardController = LocalSoftwareKeyboardController.current

    LaunchedEffect(isExpanded) {
        if (isExpanded) {
            keyboardController?.hide()
        }
    }

    Box {
        LoginPickerHeader(
            isExpanded = isExpanded,
            loginPickerColors = loginPickerColors,
            modifier = Modifier
                .clickable { onExpandToggleClick(!isExpanded) }
                .align(Alignment.TopStart),
        )

        if (isExpanded) {
            LazyColumn(
                modifier = Modifier
                    .fillMaxWidth()
                    .align(Alignment.TopStart)
                    .padding(vertical = 48.dp),
            ) {
                items(logins) { login ->
                    LoginListItem(
                        login = login,
                        loginPickerColors = loginPickerColors,
                        onListItemClicked = { onLoginSelected(login) },
                    )
                    Divider()
                }
            }

            LoginPickerFooter(
                loginPickerColors = loginPickerColors,
                modifier = Modifier
                    .clickable { onManagePasswordClicked() }
                    .align(Alignment.BottomStart),
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun LoginPreview() {
    var isExpanded by remember { mutableStateOf(true) }

    LoginPicker(
        logins = listOf(
            Login("1", "foxy-1@mozilla.com", "foxy@mozilla.com", "1"),
            Login("2", "foxy-2@mozilla.com", "foxy@mozilla.com", "1"),
            Login("3", "foxy-3@mozilla.com", "foxy@mozilla.com", "1"),
            Login("4", "foxy-4@mozilla.com", "foxy@mozilla.com", "1"),
            Login("5", "foxy-5@mozilla.com", "foxy@mozilla.com", "1"),
        ),
        isExpanded = isExpanded,
        onExpandToggleClick = { isExpanded = it },
        onLoginSelected = { },
        onManagePasswordClicked = { },
        loginPickerColors = LoginPickerColors(
            primary = MaterialTheme.colors.primary,
            header = MaterialTheme.colors.onBackground,
        ),
    )
}
