package dev.anilbeesetti.nextplayer.settings.screens.player

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.nestedscroll.nestedScroll
import androidx.compose.ui.res.stringResource
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import dev.anilbeesetti.nextplayer.core.datastore.DoubleTapGesture
import dev.anilbeesetti.nextplayer.core.datastore.FastSeek
import dev.anilbeesetti.nextplayer.core.datastore.Resume
import dev.anilbeesetti.nextplayer.core.ui.R
import dev.anilbeesetti.nextplayer.core.ui.components.ClickablePreferenceItem
import dev.anilbeesetti.nextplayer.core.ui.components.NextTopAppBar
import dev.anilbeesetti.nextplayer.core.ui.components.PreferenceSwitch
import dev.anilbeesetti.nextplayer.core.ui.components.PreferenceSwitchWithDivider
import dev.anilbeesetti.nextplayer.core.ui.components.RadioTextButton
import dev.anilbeesetti.nextplayer.core.ui.designsystem.NextIcons
import dev.anilbeesetti.nextplayer.settings.composables.OptionsDialog
import dev.anilbeesetti.nextplayer.settings.composables.PreferenceSubtitle

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PlayerPreferencesScreen(
    onNavigateUp: () -> Unit,
    viewModel: PlayerPreferencesViewModel = hiltViewModel()
) {
    val preferences by viewModel.preferencesFlow.collectAsStateWithLifecycle()
    val uiState by viewModel.uiState.collectAsStateWithLifecycle()

    val scrollBehaviour = TopAppBarDefaults.pinnedScrollBehavior()

    Scaffold(
        modifier = Modifier.nestedScroll(scrollBehaviour.nestedScrollConnection),
        topBar = {
            NextTopAppBar(
                title = stringResource(id = R.string.player_name),
                scrollBehavior = scrollBehaviour,
                navigationIcon = {
                    IconButton(onClick = onNavigateUp) {
                        Icon(
                            imageVector = NextIcons.ArrowBack,
                            contentDescription = stringResource(id = R.string.navigate_up)
                        )
                    }
                }
            )
        }
    ) { innerPadding ->
        Box(
            modifier = Modifier.fillMaxSize()
        ) {
            LazyColumn(
                contentPadding = innerPadding,
                modifier = Modifier
                    .fillMaxSize()
            ) {
                item {
                    PreferenceSubtitle(text = stringResource(id = R.string.interface_name))
                }
                item {
                    PreferenceSwitch(
                        title = stringResource(id = R.string.seek_gesture),
                        description = stringResource(id = R.string.seek_gesture_description),
                        icon = NextIcons.SwipeHorizontal,
                        isChecked = preferences.useSeekControls,
                        onClick = viewModel::toggleSeekControls
                    )
                }
                item {
                    PreferenceSwitch(
                        title = stringResource(id = R.string.swipe_gesture),
                        description = stringResource(id = R.string.swipe_gesture_description),
                        icon = NextIcons.SwipeVertical,
                        isChecked = preferences.useSwipeControls,
                        onClick = viewModel::toggleSwipeControls
                    )
                }
                item {
                    PreferenceSwitchWithDivider(
                        title = stringResource(id = R.string.double_tap),
                        description = stringResource(id = R.string.double_tap_description),
                        isChecked = (preferences.doubleTapGesture != DoubleTapGesture.NONE),
                        onChecked = viewModel::toggleDoubleTapGesture,
                        icon = NextIcons.DoubleTap,
                        onClick = {
                            viewModel.onEvent(
                                PlayerPreferencesEvent.ShowDialog(
                                    PlayerPreferenceDialog.DoubleTapDialog
                                )
                            )
                        }
                    )
                }
                item {
                    PreferenceSubtitle(text = stringResource(id = R.string.playback))
                }
                item {
                    ClickablePreferenceItem(
                        title = stringResource(id = R.string.resume),
                        description = stringResource(id = R.string.resume_description),
                        icon = NextIcons.Resume,
                        onClick = {
                            viewModel.onEvent(
                                PlayerPreferencesEvent.ShowDialog(
                                    PlayerPreferenceDialog.ResumeDialog
                                )
                            )
                        }
                    )
                }
                item {
                    PreferenceSwitch(
                        title = stringResource(id = R.string.remember_brightness_level),
                        description = stringResource(
                            id = R.string.remember_brightness_level_description
                        ),
                        icon = NextIcons.Brightness,
                        isChecked = preferences.rememberPlayerBrightness,
                        onClick = viewModel::toggleRememberBrightnessLevel
                    )
                }
                item {
                    PreferenceSwitch(
                        title = stringResource(id = R.string.remember_selections),
                        description = stringResource(id = R.string.remember_selections_description),
                        icon = NextIcons.Selection,
                        isChecked = preferences.rememberSelections,
                        onClick = viewModel::toggleRememberSelections
                    )
                }
                item {
                    PreferenceSwitchWithDivider(
                        title = stringResource(id = R.string.fast_seek),
                        description = stringResource(id = R.string.fast_seek_description),
                        isChecked = (preferences.fastSeek != FastSeek.DISABLE),
                        onChecked = viewModel::toggleFastSeek,
                        icon = NextIcons.Fast,
                        onClick = {
                            viewModel.onEvent(
                                PlayerPreferencesEvent.ShowDialog(
                                    PlayerPreferenceDialog.FastSeekDialog
                                )
                            )
                        }
                    )
                }
            }
            when (uiState.showDialog) {
                PlayerPreferenceDialog.ResumeDialog -> {
                    OptionsDialog(
                        text = stringResource(id = R.string.resume),
                        onDismissClick = {
                            viewModel.onEvent(
                                PlayerPreferencesEvent.ShowDialog(PlayerPreferenceDialog.None)
                            )
                        }
                    ) {
                        Resume.values().map {
                            RadioTextButton(
                                text = it.value,
                                selected = (it == preferences.resume),
                                onClick = {
                                    viewModel.updatePlaybackResume(it)
                                    viewModel.onEvent(
                                        PlayerPreferencesEvent.ShowDialog(
                                            PlayerPreferenceDialog.None
                                        )
                                    )
                                }
                            )
                        }
                    }
                }
                PlayerPreferenceDialog.DoubleTapDialog -> {
                    OptionsDialog(
                        text = stringResource(id = R.string.double_tap),
                        onDismissClick = {
                            viewModel.onEvent(
                                PlayerPreferencesEvent.ShowDialog(PlayerPreferenceDialog.None)
                            )
                        }
                    ) {
                        DoubleTapGesture.values().forEach {
                            RadioTextButton(
                                text = it.value,
                                selected = (it == preferences.doubleTapGesture),
                                onClick = {
                                    viewModel.updateDoubleTapGesture(it)
                                    viewModel.onEvent(
                                        PlayerPreferencesEvent.ShowDialog(
                                            PlayerPreferenceDialog.None
                                        )
                                    )
                                }
                            )
                        }
                    }
                }
                PlayerPreferenceDialog.FastSeekDialog -> {
                    OptionsDialog(
                        text = stringResource(id = R.string.fast_seek),
                        onDismissClick = {
                            viewModel.onEvent(
                                PlayerPreferencesEvent.ShowDialog(PlayerPreferenceDialog.None)
                            )
                        }
                    ) {
                        FastSeek.values().forEach {
                            RadioTextButton(
                                text = it.value,
                                selected = (it == preferences.fastSeek),
                                onClick = {
                                    viewModel.updateFastSeek(it)
                                    viewModel.onEvent(
                                        PlayerPreferencesEvent.ShowDialog(
                                            PlayerPreferenceDialog.None
                                        )
                                    )
                                }
                            )
                        }
                    }
                }
                PlayerPreferenceDialog.None -> { /* Do nothing */ }
            }
        }
    }
}
