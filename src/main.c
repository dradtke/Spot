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

static sp_session *g_session;
static audio_fifo_t g_audiofifo;
static bool logged_in = 0;
static bool running = 1;

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

static void play(sp_track *track)
{
	sp_error error;
	if ((error = sp_session_player_load(g_session, track)) != SP_ERROR_OK) {
		fprintf(stderr, "Failed to load the player.\n");
		exit(2);
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

	sp_session_player_play(g_session, 1);
}

static void on_search(sp_search *search, void *userdata)
{
	debug("on_search() called\n");
	if (sp_search_error(search) != SP_ERROR_OK) {
		debug("failing\n");
		fprintf(stderr, "Failed to search, figure out how to get the error message.\n");
		exit(2);
	}

	int num_tracks = sp_search_num_tracks(search);
	if (num_tracks == 0) {
		fprintf(stderr, "Sorry, couldn't find that track. =/\n");
		running = 0;
		return;
	}

	sp_track *track = sp_search_track(search, 0);
	play(track);
}

static void on_login(sp_session *sess, sp_error error)
{
	debug("on_login() called\n");
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "Login failed: %s\n", sp_error_message(error));
		exit(2);
	}

	logged_in = 1;

}

static void on_notify_main_thread(sp_session *sess)
{
	debug("on_notify_main_thread() called\n");
}

/*
 * This method was taken from share/doc/libspotify/examples/jukebox/jukebox.c
 */
static int on_music_delivery(sp_session *sess, const sp_audioformat *format, const void *frames, int num_frames)
{
	debug("on_music_delivery() called\n");
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

static void on_metadata_updated(sp_session *sess)
{
	debug("on_metadata_updated() called\n");
}

static void on_play_token_lost(sp_session *sess)
{
	debug("on_play_token_lost() called\n");
	audio_fifo_flush(&g_audiofifo);
}

static void on_end_of_track(sp_session *sess)
{
	debug("on_end_of_track() called\n");
	audio_fifo_flush(&g_audiofifo);
	running = 0;
	printf("Done.\n");
}

static sp_session_callbacks session_callbacks = {
	.logged_in = &on_login,
	.notify_main_thread = &on_notify_main_thread,
	.music_delivery = &on_music_delivery,
	.metadata_updated = &on_metadata_updated,
	.play_token_lost = &on_play_token_lost,
	.log_message = NULL,
	.end_of_track = &on_end_of_track
};

static sp_session_config spconfig = {
	.api_version = SPOTIFY_API_VERSION,
	.cache_location = "tmp",
	.settings_location = "tmp",
	.application_key = g_appkey,
	.application_key_size = 0, // set in main()
	.user_agent = "libspotify-test",
	.callbacks = &session_callbacks,
	NULL
};

int main(void)
{
	int i;
	sp_error err;

	// create the spotify session
	spconfig.application_key_size = g_appkey_size;
	err = sp_session_create(&spconfig, &g_session);
	if (err != SP_ERROR_OK) {
		fprintf(stderr, "Unable to create session: %s\n", sp_error_message(err));
		return 1;
	}

	// initialize audio
	audio_init(&g_audiofifo);

	// log in
	int next_timeout = 0;
	sp_session_login(g_session, username, password, 0, NULL);
	while (!logged_in) {
		sp_session_process_events(g_session, &next_timeout);
	}

	// ask the user for an artist and track
	char artist[1024];
	printf("Artist: ");
	fgets(artist, 1024, stdin);
	artist[strlen(artist)-1] = '\0';
	for (i = 0; i<strlen(artist); i++) {
		if (artist[i] == ' ')
			artist[i] = '+';
	}

	char track[1024];
	printf("Track: ");
	fgets(track, 1024, stdin);
	track[strlen(track)-1] = '\0';
	for (i = 0; i<strlen(track); i++) {
		if (track[i] == ' ')
			track[i] = '+';
	}

	char q[4096];
	sprintf(q, "artist:%s track:%s", artist, track);

	// start the search
	sp_search_create(g_session, q, 0, 250, 0, 0, 0, 0, 0, 0, SP_SEARCH_STANDARD, &on_search, NULL);

	// main loop
	while (running) {
		sp_session_process_events(g_session, &next_timeout);
	}

	// TODO: clean up

	return 0;
}

