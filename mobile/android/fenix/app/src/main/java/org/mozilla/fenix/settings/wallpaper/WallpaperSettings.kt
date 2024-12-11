/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.wallpaper

import android.annotation.SuppressLint
import android.graphics.Bitmap
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.CircularProgressIndicator
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.SemanticsPropertyReceiver
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.onClick
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.style.TextDecoration
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.ClickableSubstringLink
import org.mozilla.fenix.compose.ext.debouncedClickable
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.wallpapers.Wallpaper

/**
 * The screen for controlling settings around Wallpapers. When a new wallpaper is selected,
 * a snackbar will be displayed.
 *
 * @param wallpaperGroups Wallpapers groups to add to grid.
 * @param defaultWallpaper The default wallpaper.
 * @param loadWallpaperResource Callback to handle loading a wallpaper bitmap. Only optional in the default case.
 * @param selectedWallpaper The currently selected wallpaper.
 * @param onSelectWallpaper Callback for when a new wallpaper is selected.
 * @param onLearnMoreClick Callback for when the learn more action is clicked from the group description.
 * Parameters are the URL that is clicked and the name of the collection.
 */
@SuppressLint("UnusedMaterialScaffoldPaddingParameter")
@Composable
fun WallpaperSettings(
    wallpaperGroups: Map<Wallpaper.Collection, List<Wallpaper>>,
    defaultWallpaper: Wallpaper,
    loadWallpaperResource: suspend (Wallpaper) -> Bitmap?,
    selectedWallpaper: Wallpaper,
    onSelectWallpaper: (Wallpaper) -> Unit,
    onLearnMoreClick: (String, String) -> Unit,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .background(color = FirefoxTheme.colors.layer1)
            .padding(horizontal = FirefoxTheme.space.baseContentHorizontalPadding),
    ) {
        Spacer(modifier = Modifier.height(FirefoxTheme.space.baseContentVerticalPadding))

        wallpaperGroups.forEach { (collection, wallpapers) ->
            if (wallpapers.isNotEmpty()) {
                WallpaperGroupHeading(
                    collection = collection,
                    onLearnMoreClick = onLearnMoreClick,
                )

                Spacer(modifier = Modifier.height(FirefoxTheme.space.small))

                WallpaperThumbnails(
                    wallpapers = wallpapers,
                    defaultWallpaper = defaultWallpaper,
                    loadWallpaperResource = loadWallpaperResource,
                    selectedWallpaper = selectedWallpaper,
                    onSelectWallpaper = onSelectWallpaper,
                )

                Spacer(modifier = Modifier.height(FirefoxTheme.space.large))
            }
        }
    }
}

@Composable
@Suppress("Deprecation")
private fun WallpaperGroupHeading(
    collection: Wallpaper.Collection,
    onLearnMoreClick: (String, String) -> Unit,
) {
    // Since the last new collection of wallpapers was tied directly to an MR release,
    // it was decided that we should use string resources for these titles
    // and descriptions so they could be localized.
    // In the future, we may want to either use the dynamic wallpaper properties with localized fallbacks
    // or invest in a method of localizing the remote strings themselves.
    if (collection.name == Wallpaper.classicFirefoxCollectionName) {
        Text(
            text = stringResource(R.string.wallpaper_classic_title, stringResource(R.string.firefox)),
            color = FirefoxTheme.colors.textSecondary,
            style = FirefoxTheme.typography.subtitle2,
        )
    } else {
        val label = stringResource(id = R.string.a11y_action_label_wallpaper_collection_learn_more)
        val headingSemantics: SemanticsPropertyReceiver.() -> Unit =
            if (collection.learnMoreUrl.isNullOrEmpty()) {
                {}
            } else {
                {
                    role = Role.Button
                    onClick(label = label) {
                        onLearnMoreClick(collection.learnMoreUrl, collection.name)
                        false
                    }
                }
            }
        Column(
            modifier = Modifier.semantics(mergeDescendants = true, properties = headingSemantics),
        ) {
            Text(
                text = stringResource(R.string.wallpaper_artist_series_title),
                color = FirefoxTheme.colors.textSecondary,
                style = FirefoxTheme.typography.subtitle2,
            )

            if (collection.learnMoreUrl.isNullOrEmpty()) {
                val text = stringResource(R.string.wallpaper_artist_series_description)
                Text(
                    text = text,
                    color = FirefoxTheme.colors.textSecondary,
                    style = FirefoxTheme.typography.caption,
                )
            } else {
                val link = stringResource(R.string.wallpaper_learn_more)
                val text = stringResource(R.string.wallpaper_artist_series_description_with_learn_more, link)
                val linkStartIndex = text.indexOf(link)
                val linkEndIndex = linkStartIndex + link.length

                ClickableSubstringLink(
                    text = text,
                    textColor = FirefoxTheme.colors.textSecondary,
                    linkTextColor = FirefoxTheme.colors.textAccent,
                    linkTextDecoration = TextDecoration.Underline,
                    clickableStartIndex = linkStartIndex,
                    clickableEndIndex = linkEndIndex,
                ) {
                    onLearnMoreClick(collection.learnMoreUrl, collection.name)
                }
            }
        }
    }
}

/**
 * A grid of selectable wallpaper thumbnails.
 *
 * @param wallpapers Wallpapers to add to grid.
 * @param defaultWallpaper The default wallpaper.
 * @param selectedWallpaper The currently selected wallpaper.
 * @param loadWallpaperResource Callback to handle loading a wallpaper bitmap. Only optional in the default case.
 * @param onSelectWallpaper Action to take when a new wallpaper is selected.
 */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun WallpaperThumbnails(
    wallpapers: List<Wallpaper>,
    defaultWallpaper: Wallpaper,
    selectedWallpaper: Wallpaper,
    loadWallpaperResource: suspend (Wallpaper) -> Bitmap?,
    onSelectWallpaper: (Wallpaper) -> Unit,
) {
    FlowRow(
        horizontalArrangement = Arrangement.spacedBy(FirefoxTheme.space.xSmall),
        verticalArrangement = Arrangement.spacedBy(FirefoxTheme.space.xSmall),
    ) {
        wallpapers.forEach { wallpaper ->
            WallpaperThumbnailItem(
                wallpaper = wallpaper,
                defaultWallpaper = defaultWallpaper,
                loadWallpaperResource = loadWallpaperResource,
                isSelected = selectedWallpaper.name == wallpaper.name,
                isLoading = wallpaper.assetsFileState == Wallpaper.ImageFileState.Downloading,
                onSelect = onSelectWallpaper,
            )
        }
    }
}

/**
 * A single wallpaper thumbnail.
 *
 * @param wallpaper The wallpaper to display.
 * @param defaultWallpaper The default wallpaper.
 * @param loadWallpaperResource Callback to handle loading a wallpaper bitmap.
 * @param isSelected Whether the wallpaper is currently selected.
 * @param isLoading Whether the wallpaper is currently downloading.
 * @param aspectRatio The ratio of height to width of the thumbnail.
 * @param loadingOpacity Opacity of the currently downloading wallpaper.
 * @param onSelect Action to take when a new wallpaper is selected.
 */
@Composable
private fun WallpaperThumbnailItem(
    wallpaper: Wallpaper,
    defaultWallpaper: Wallpaper,
    loadWallpaperResource: suspend (Wallpaper) -> Bitmap?,
    isSelected: Boolean,
    isLoading: Boolean,
    aspectRatio: Float = 1.1f,
    loadingOpacity: Float = 0.5f,
    onSelect: (Wallpaper) -> Unit,
) {
    var bitmap: Bitmap? by remember { mutableStateOf(null) }
    LaunchedEffect(LocalConfiguration.current.orientation) {
        bitmap = loadWallpaperResource(wallpaper)
    }
    val border = if (isSelected) {
        BorderStroke(width = FirefoxTheme.size.border.thick, color = FirefoxTheme.colors.borderAccent)
    } else if (wallpaper.name == Wallpaper.defaultName) {
        BorderStroke(width = FirefoxTheme.size.border.thick, color = FirefoxTheme.colors.borderPrimary)
    } else {
        null
    }

    // Completely avoid drawing the item if a bitmap cannot be loaded and is required
    if (bitmap != null || wallpaper == defaultWallpaper) {
        val description = stringResource(
            R.string.wallpapers_item_name_content_description,
            wallpaper.name,
        )

        // For the default wallpaper to be accessible, we should set the content description for
        // the Surface instead of the thumbnail image
        val contentDescriptionModifier = if (bitmap == null) {
            Modifier.semantics {
                contentDescription = description
            }
        } else {
            Modifier
        }

        Surface(
            modifier = Modifier
                .width(width = FirefoxTheme.size.xxLarge)
                .aspectRatio(aspectRatio)
                .debouncedClickable { onSelect(wallpaper) }
                .then(contentDescriptionModifier),
            shape = RoundedCornerShape(size = FirefoxTheme.size.corner.large),
            border = border,
            elevation = FirefoxTheme.size.elevation.medium,
        ) {
            if (bitmap == null) {
                Spacer(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(color = FirefoxTheme.colors.layer1),
                )
            } else {
                bitmap?.let {
                    Image(
                        bitmap = it.asImageBitmap(),
                        contentScale = ContentScale.FillBounds,
                        contentDescription = description,
                        modifier = Modifier.fillMaxSize(),
                        alpha = if (isLoading) loadingOpacity else 1.0f,
                    )
                }
            }

            if (isLoading) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center,
                ) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(FirefoxTheme.size.circularIndicatorDiameter),
                        color = FirefoxTheme.colors.borderAccent,
                    )
                }
            }
        }
    }
}

@FlexibleWindowLightDarkPreview
@Composable
@Suppress("MagicNumber")
private fun WallpaperThumbnailsPreview() {
    val downloadedCollection: List<Wallpaper> = List(size = 5) { index ->
        Wallpaper(
            name = "downloaded wallpaper $index",
            textColor = 0L,
            cardColorLight = 0L,
            cardColorDark = 0L,
            thumbnailFileState = Wallpaper.ImageFileState.Downloaded,
            assetsFileState = Wallpaper.ImageFileState.Downloaded,
            collection = Wallpaper.ClassicFirefoxCollection,
        )
    } + Wallpaper.Default
    val downloadingCollection: List<Wallpaper> = List(size = 5) { index ->
        Wallpaper(
            name = "downloading wallpaper $index",
            textColor = 0L,
            cardColorLight = 0L,
            cardColorDark = 0L,
            thumbnailFileState = Wallpaper.ImageFileState.Downloading,
            assetsFileState = Wallpaper.ImageFileState.Downloading,
            collection = Wallpaper.ClassicFirefoxCollection,
        )
    }
    var selectedWallpaper by remember { mutableStateOf(downloadedCollection[0]) }

    FirefoxTheme {
        WallpaperSettings(
            defaultWallpaper = Wallpaper.Default,
            loadWallpaperResource = { wallpaper ->
                if (wallpaper == Wallpaper.Default) {
                    null
                } else {
                    Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888)
                }
            },
            wallpaperGroups = mapOf(
                Wallpaper.DefaultCollection to downloadedCollection,
                Wallpaper.ClassicFirefoxCollection to downloadingCollection,
            ),
            selectedWallpaper = selectedWallpaper,
            onSelectWallpaper = { wallpaper ->
                selectedWallpaper = wallpaper
            },
            onLearnMoreClick = { _, _ -> },
        )
    }
}
