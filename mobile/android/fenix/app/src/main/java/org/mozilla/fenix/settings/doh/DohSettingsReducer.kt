/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

internal fun dohSettingsReducer(state: DohSettingsState, action: DohSettingsAction) =
    when (action) {
        is DohSettingsRootAction.DohOptionSelected -> {
            val selectedProvider = when (action.protectionLevel) {
                is ProtectionLevel.Increased,
                is ProtectionLevel.Max,
                -> action.provider ?: state.providers.first()

                else -> null
            }
            state.copy(
                selectedProtectionLevel = action.protectionLevel,
                selectedProvider = selectedProvider,
            )
        }

        is DohSettingsRootAction.DohSettingsLoaded -> {
            state.copy(
                allProtectionLevels = action.allProtectionLevels,
                selectedProtectionLevel = action.selectedProtectionLevel,
                exceptionsList = action.exceptionsList,
                providers = action.providers,
                selectedProvider = action.selectedProvider,
            )
        }

        is DohSettingsRootAction.CustomClicked -> {
            state.copy(
                isCustomProviderDialogOn = true,
            )
        }

        is DohSettingsRootAction.DohCustomProviderDialogAction.NonHttpsUrlDetected -> {
            state.copy(
                isCustomProviderDialogOn = true,
                customProviderErrorState = CustomProviderErrorState.NonHttps,
            )
        }

        is DohSettingsRootAction.DohCustomProviderDialogAction.InvalidUrlDetected -> {
            state.copy(
                isCustomProviderDialogOn = true,
                customProviderErrorState = CustomProviderErrorState.Invalid,
            )
        }

        is DohSettingsRootAction.DohCustomProviderDialogAction.ValidUrlDetected -> {
            state.copy(
                isCustomProviderDialogOn = false,
                customProviderErrorState = CustomProviderErrorState.Valid,
                selectedProvider = action.customProvider.copy(
                    url = action.url,
                ),
            )
        }

        is DohSettingsRootAction.DohCustomProviderDialogAction.CancelClicked -> {
            state.copy(
                isCustomProviderDialogOn = false,
                customProviderErrorState = CustomProviderErrorState.Valid,
            )
        }

        is ExceptionsAction.ExceptionsUpdated -> {
            state.copy(
                exceptionsList = action.exceptionsList,
                isUserExceptionValid = true,
            )
        }

        is ExceptionsAction.InvalidUrlDetected -> {
            state.copy(isUserExceptionValid = false)
        }

        else -> {
            // do nothing
            state
        }
    }
