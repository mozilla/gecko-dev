/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.ui

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandHorizontally
import androidx.compose.animation.fadeOut
import androidx.compose.animation.shrinkHorizontally
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.draggable2D
import androidx.compose.foundation.gestures.rememberDraggable2DState
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.DrawerValue
import androidx.compose.material3.ModalDrawerSheet
import androidx.compose.material3.Text
import androidx.compose.material3.rememberDrawerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.navigation.NavHostController
import androidx.navigation.compose.rememberNavController
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.FloatingActionButton
import org.mozilla.fenix.debugsettings.navigation.DebugDrawerDestination
import org.mozilla.fenix.debugsettings.store.DrawerStatus
import org.mozilla.fenix.theme.FirefoxTheme
import kotlin.math.roundToInt

/**
 * The initial x offset of the debug drawer FAB from [Alignment.CenterStart].
 */
private const val INITIAL_FAB_OFFSET_X = 0f

/**
 * The initial y offset of the debug drawer FAB from [Alignment.CenterStart].
 */
private const val INITIAL_FAB_OFFSET_Y = 0f

/**
 * Overlay for presenting app-wide debugging content.
 *
 * @param navController [NavHostController] used to perform navigation actions.
 * @param drawerStatus The [DrawerStatus] indicating the physical state of the drawer.
 * @param debugDrawerDestinations The complete list of [DebugDrawerDestination]s used to populate
 * the [DebugDrawer] with sub screens.
 * @param onDrawerOpen Invoked when the drawer is opened.
 * @param onDrawerClose Invoked when the drawer is closed.
 * @param onDrawerBackButtonClick Invoked when the user taps on the back button in the app bar.
 */
@OptIn(ExperimentalFoundationApi::class)
@Suppress("LongMethod")
@Composable
fun DebugOverlay(
    navController: NavHostController,
    drawerStatus: DrawerStatus,
    debugDrawerDestinations: List<DebugDrawerDestination>,
    onDrawerOpen: () -> Unit,
    onDrawerClose: () -> Unit,
    onDrawerBackButtonClick: () -> Unit,
) {
    val drawerState = rememberDrawerState(initialValue = DrawerValue.Closed)
    var debugDrawerFabOffsetX by remember { mutableFloatStateOf(INITIAL_FAB_OFFSET_X) }
    var debugDrawerFabOffsetY by remember { mutableFloatStateOf(INITIAL_FAB_OFFSET_Y) }
    val density = LocalDensity.current

    BackHandler(enabled = drawerStatus == DrawerStatus.Open) {
        onDrawerClose()
    }

    Box(
        modifier = Modifier.fillMaxSize(),
    ) {
        FloatingActionButton(
            icon = painterResource(R.drawable.ic_debug_transparent_fire_24),
            modifier = Modifier
                .align(Alignment.CenterStart)
                .padding(start = 16.dp)
                .offset { IntOffset(debugDrawerFabOffsetX.roundToInt(), debugDrawerFabOffsetY.roundToInt()) }
                .draggable2D(
                    state = rememberDraggable2DState { offset ->
                        debugDrawerFabOffsetX += offset.x
                        debugDrawerFabOffsetY += offset.y
                    },
                ),
            onClick = {
                onDrawerOpen()
            },
            contentDescription = stringResource(R.string.debug_drawer_fab_content_description),
        )

        AnimatedVisibility(
            visible = drawerStatus == DrawerStatus.Open,
            enter = slideInHorizontally {
                with(density) { -40.dp.roundToPx() }
            } + expandHorizontally(
                expandFrom = Alignment.Start,
            ),
            exit = slideOutHorizontally() + shrinkHorizontally() + fadeOut(),
        ) {
            Row {
                ModalDrawerSheet(
                    drawerContainerColor = FirefoxTheme.colors.layer1,
                    drawerState = drawerState,
                ) {
                    DebugDrawer(
                        navController = navController,
                        destinations = debugDrawerDestinations,
                        onBackButtonClick = onDrawerBackButtonClick,
                    )
                }

                Box(modifier = Modifier.weight(1f).fillMaxHeight().clickable(onClick = onDrawerClose))
            }
        }
    }
}

@Composable
@PreviewLightDark
private fun DebugOverlayPreview() {
    val navController = rememberNavController()
    var drawerStatus by remember { mutableStateOf(DrawerStatus.Closed) }
    val destinations = remember {
        List(size = 15) { index ->
            DebugDrawerDestination(
                route = "screen_$index",
                title = R.string.debug_drawer_title,
                onClick = {
                    navController.navigate(route = "screen_$index")
                },
                content = {
                    Text(
                        text = "Tool $index",
                        color = FirefoxTheme.colors.textPrimary,
                        style = FirefoxTheme.typography.headline6,
                    )
                },
            )
        }
    }

    FirefoxTheme {
        DebugOverlay(
            navController = navController,
            drawerStatus = drawerStatus,
            debugDrawerDestinations = destinations,
            onDrawerOpen = {
                drawerStatus = DrawerStatus.Open
            },
            onDrawerClose = {
                drawerStatus = DrawerStatus.Closed
            },
            onDrawerBackButtonClick = {
                navController.popBackStack()
            },
        )
    }
}
