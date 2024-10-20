package dev.anilbeesetti.nextplayer.feature.player.extensions

import androidx.media3.common.C
import androidx.media3.common.Player
import androidx.media3.common.TrackSelectionOverride
import androidx.media3.common.util.UnstableApi
import androidx.media3.exoplayer.ExoPlayer
import androidx.media3.exoplayer.SeekParameters
import timber.log.Timber

/**
 * Switches to selected track.
 *
 * @param trackType The type of track to switch.
 * @param trackIndex The index of the track to switch to, or null to enable the track.
 *
 * if trackIndex is -1, the track will be disabled
 * if trackIndex is null, the track will be enabled
 * if trackIndex is a valid index, the track will be switched to that index
 *
 */
fun Player.switchTrack(trackType: @C.TrackType Int, trackIndex: Int?) {
    val trackTypeText = when (trackType) {
        C.TRACK_TYPE_AUDIO -> "audio"
        C.TRACK_TYPE_TEXT -> "subtitle"
        else -> throw IllegalArgumentException("Invalid track type: $trackType")
    }
    if (trackIndex != null) {
        if (trackIndex == -1) {
            Timber.d("Disabling $trackTypeText")
            trackSelectionParameters = trackSelectionParameters
                .buildUpon()
                .setTrackTypeDisabled(trackType, true)
                .build()
        } else {
            Timber.d("Setting $trackTypeText track: $trackIndex")

            val tracks = currentTracks.groups.filter { it.type == trackType }

            if (tracks.isEmpty() || trackIndex >= tracks.size) {
                Timber.d("Operation failed: Invalid track index: $trackIndex")
                return
            }

            val trackSelectionOverride = TrackSelectionOverride(
                tracks[trackIndex].mediaTrackGroup,
                0
            )

            // Override the track selection parameters to force the selection of the specified track.
            trackSelectionParameters = trackSelectionParameters
                .buildUpon()
                .setTrackTypeDisabled(trackType, false)
                .setOverrideForType(trackSelectionOverride)
                .build()
        }
    } else {
        // Enable the track
        trackSelectionParameters = trackSelectionParameters
            .buildUpon()
            .setTrackTypeDisabled(trackType, false)
            .build()
    }
}

/**
 * Sets the seek parameters for the player.
 *
 * @param seekParameters The seek parameters to set.
 */
@UnstableApi
fun Player.setSeekParameters(seekParameters: SeekParameters) {
    when (this) {
        is ExoPlayer -> this.setSeekParameters(seekParameters)
    }
}

/**
 * Seeks to the specified position.
 *
 * @param positionMs The position to seek to, in milliseconds.
 * @param fastSeek Whether to seek to the nearest keyframe.
 */
@UnstableApi
fun Player.seekBack(positionMs: Long, fastSeek: Boolean = false) {
    if (fastSeek) this.setSeekParameters(SeekParameters.PREVIOUS_SYNC)
    this.seekTo(positionMs)
}

/**
 * Seeks to the specified position.
 *
 * @param positionMs The position to seek to, in milliseconds.
 * @param fastSeek Whether to seek to the nearest keyframe.
 */
@UnstableApi
fun Player.seekForward(positionMs: Long, fastSeek: Boolean = false) {
    if (fastSeek) this.setSeekParameters(SeekParameters.NEXT_SYNC)
    this.seekTo(positionMs)
}
