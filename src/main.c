/*
 * Copyright (c) 2012 Damien Radtke <damienradtke@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libspotify/api.h>
#include "audio.h"

#define DEBUG 0

extern const uint8_t g_appkey[];
extern const size_t g_appkey_size;
extern const char *username;
extern const char *password;

static audio_fifo_t g_audiofifo;
static bool logged_in = 0;

void debug(const char *format, ...)
{
	if (!DEBUG)
		return;

	va_list argptr;
	va_start(argptr, format);
	vprintf(format, argptr);
	printf("\n");
}

void format_duration(char *s, int duration)
{
	int minutes = duration / (1000*60);
	int seconds = duration / 1000;
	char str_seconds[3];
	snprintf(str_seconds, 3, "%02d", seconds);

	sprintf(s, "%02d:%s", minutes, str_seconds);
}

void play(sp_session *session, sp_track *track)
{
	sp_error error = sp_session_player_load(session, track);
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "Error: %s\n", sp_error_message(error));
		exit(1);
	}

	sp_artist *artist = sp_track_artist(track, 0);
	sp_album *album = sp_track_album(track);
	char duration[1024];
	format_duration(duration, sp_track_duration(track));

	printf("\n");
	printf("       Track: %s\n", sp_track_name(track));
	printf("      Artist: %s\n", sp_artist_name(artist));
	printf("       Album: %s\n", sp_album_name(album));
	printf("        Year: %d\n", sp_album_year(album));
	printf("  Popularity: %d / 100\n", sp_track_popularity(track));
	printf("    Duration: %s\n", duration);
	printf("\n");
	printf("Playing...\n");

	sp_session_player_play(session, 1);
}

static void on_search_complete(sp_search *search, void *userdata)
{
	debug("callback: on_search_complete");
	sp_error error = sp_search_error(search);
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "Error: %s\n", sp_error_message(error));
		exit(1);
	}

	int num_tracks = sp_search_num_tracks(search);
	if (num_tracks == 0) {
		fprintf(stderr, "Sorry, couldn't find that track. =/\n");
		exit(0);
	}

	sp_track *track = sp_search_track(search, 0);
	play((sp_session*)userdata, track);
}

void run_search(sp_session *session)
{
	// ask the user for an artist and track
	char artist[1024];
	printf("Artist: ");
	fgets(artist, 1024, stdin);
	artist[strlen(artist)-1] = '\0';

	char track[1024];
	printf("Track: ");
	fgets(track, 1024, stdin);
	track[strlen(track)-1] = '\0';

	char q[4096];
	sprintf(q, "artist:\"%s\" track:\"%s\"", artist, track);

	// start the search
	sp_search_create(session, q, 0, 1, 0, 0, 0, 0, 0, 0, SP_SEARCH_STANDARD, &on_search_complete, session);
}

static void on_login(sp_session *session, sp_error error)
{
	debug("callback: on_login");
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "Login failed: %s\n", sp_error_message(error));
		exit(2);
	}

	logged_in = 1;
	run_search(session);
}

static void on_notify_main_thread(sp_session *sess)
{
	debug("callback: on_notif_main_thread");
}

/*
 * This method was taken from share/doc/libspotify/examples/jukebox/jukebox.c
 */
static int on_music_delivery(sp_session *sess, const sp_audioformat *format, const void *frames, int num_frames)
{
	debug("callback: on_music_delivery");
	audio_fifo_t *af = &g_audiofifo;
	audio_fifo_data_t *afd;
	size_t s;

	// audio discontinuity, do nothing
	if (num_frames == 0) {
		pthread_mutex_unlock(&af->mutex);
		return 0;
	}

	// buffer one second of audio
	if (af->qlen > format->sample_rate)
		return 0;

	s = num_frames * sizeof(int16_t) * format->channels;
	afd = malloc(sizeof(*afd) + s);
	memcpy(afd->samples, frames, s);

	afd->nsamples = num_frames;
	afd->rate = format->sample_rate;
	afd->channels = format->channels;

	TAILQ_INSERT_TAIL(&af->q, afd, link);
	af->qlen += num_frames;

	pthread_cond_signal(&af->cond);
	pthread_mutex_unlock(&af->mutex);

	return num_frames;
}

static void on_end_of_track(sp_session *sess)
{
	debug("callback: on_end_of_track");
	audio_fifo_flush(&g_audiofifo);
	printf("Done.\n");
	exit(0);
}

static void on_log(sp_session *sess, const char *data)
{
	//debug("spotify: %s\n", data);
}

static sp_session_callbacks session_callbacks = {
	.logged_in = &on_login,
	.notify_main_thread = &on_notify_main_thread,
	.music_delivery = &on_music_delivery,
	.log_message = &on_log,
	.end_of_track = &on_end_of_track
};

static sp_session_config spconfig = {
	.api_version = SPOTIFY_API_VERSION,
	.cache_location = "tmp",
	.settings_location = "tmp",
	.application_key = g_appkey,
	.application_key_size = 0, // set in main()
	.user_agent = "spot",
	.callbacks = &session_callbacks,
	NULL
};

int main(void)
{
	sp_error error;
	sp_session *session;

	// create the spotify session
	spconfig.application_key_size = g_appkey_size;
	error = sp_session_create(&spconfig, &session);
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "Unable to create session: %s\n", sp_error_message(error));
		return 1;
	}

	// initialize audio
	audio_init(&g_audiofifo);

	// log in
	int next_timeout = 0;
	sp_session_login(session, username, password, 0, NULL);

	// main loop
	while (1) {
		sp_session_process_events(session, &next_timeout);
		usleep(next_timeout * 1000);
	}

	return 0;
}

