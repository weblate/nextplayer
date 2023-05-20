package dev.anilbeesetti.nextplayer.feature.videopicker.screens.foldervideo

import androidx.lifecycle.SavedStateHandle
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import dev.anilbeesetti.nextplayer.core.domain.GetSortedVideosUseCase
import dev.anilbeesetti.nextplayer.feature.videopicker.MediaState
import dev.anilbeesetti.nextplayer.feature.videopicker.navigation.FolderArgs
import kotlinx.coroutines.delay
import javax.inject.Inject
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn

@HiltViewModel
class FolderVideoPickerViewModel @Inject constructor(
    getSortedVideosUseCase: GetSortedVideosUseCase,
    savedStateHandle: SavedStateHandle
) : ViewModel() {

    private val folderArgs = FolderArgs(savedStateHandle)

    val folderPath = folderArgs.folderId

    val videoItems = getSortedVideosUseCase.invoke(folderPath)
        .map {
            //To prevent jitter in the UI, a delay of 100 milliseconds is introduced before updating and rendering.
            delay(100)
            MediaState.Success(it)
        }
        .stateIn(
            scope = viewModelScope,
            started = SharingStarted.WhileSubscribed(5000),
            initialValue = MediaState.Loading
        )
}
