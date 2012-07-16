#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libspotify/api.h>
#include "audio.h"

extern const uint8_t g_appkey[];
extern const size_t g_appkey_size;
extern const char *username;
extern const char *password;

static sp_session *g_session;
static audio_fifo_t g_audiofifo;
static bool logged_in = 0;
static bool playing = 0;

static void on_album_browse(sp_albumbrowse *result, void *userdata)
{
	printf("on_album_browse() called\n");
	sp_error error;
	if ((error = sp_albumbrowse_error(result)) != SP_ERROR_OK) {
		fprintf(stderr, "Failed to browse album, figure out how to get the error message.\n");
		exit(2);
	}

	sp_track *royal_we = sp_albumbrowse_track(result, 1);
	if ((error = sp_session_player_load(g_session, royal_we)) != SP_ERROR_OK) {
		fprintf(stderr, "Shit, failed to load.\n");
		exit(2);
	}

	sp_session_player_play(g_session, playing = 1);
}

static void on_search(sp_search *search, void *userdata)
{
	printf("on_search() called\n");
	if (sp_search_error(search) != SP_ERROR_OK) {
		fprintf(stderr, "Failed to search, figure out how to get the error message.\n");
		exit(2);
	}

	sp_album *swoon = sp_search_album(search, 2);
	sp_albumbrowse_create(g_session, swoon, &on_album_browse, NULL);
}

static void on_login(sp_session *sess, sp_error error)
{
	printf("on_login() called\n");
	if (error != SP_ERROR_OK) {
		fprintf(stderr, "Login failed: %s\n", sp_error_message(error));
		exit(2);
	}

	logged_in = 1;

	// start the search
	sp_search_create(g_session, "silversun pickups", 0, 0, 0, 250, 0, 0, 0, 0, SP_SEARCH_STANDARD, &on_search, NULL);
}

static void on_notify_main_thread(sp_session *sess)
{
	printf("on_notify_main_thread() called\n");
}

static int on_music_delivery(sp_session *sess, const sp_audioformat *format, const void *frames, int num_frames)
{
	printf("on_music_delivery() called\n");
	audio_fifo_t *af = &g_audiofifo;
	audio_fifo_data_t *afd;
	size_t s;

	// audio discontinuity, do nothing
	if (num_frames == 0)
		return 0;

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

	return num_frames;
}

static void on_metadata_updated(sp_session *sess)
{
	printf("on_metadata_updated() called\n");
}

static void on_play_token_lost(sp_session *sess)
{
	printf("on_play_token_lost() called\n");
	audio_fifo_flush(&g_audiofifo);
}

static void on_end_of_track(sp_session *sess)
{
	printf("on_end_of_track() called\n");
	audio_fifo_flush(&g_audiofifo);
	exit(0);
}

// TODO: get this working?
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
	.cache_location = "/tmp",
	.settings_location = "/tmp",
	.application_key = g_appkey,
	.application_key_size = 0, // set in main()
	.user_agent = "libspotify-test",
	.callbacks = &session_callbacks,
	NULL
};

int main(void)
{
	sp_error err;
	int next_timeout = 0;

	spconfig.application_key_size = g_appkey_size;
	err = sp_session_create(&spconfig, &g_session);
	if (err != SP_ERROR_OK) {
		fprintf(stderr, "Unable to create session: %s\n", sp_error_message(err));
		return 1;
	}

	audio_init(&g_audiofifo);
	sp_session_login(g_session, username, password, 0, NULL);

	while (1) {
		sp_session_process_events(g_session, &next_timeout);
		if (logged_in) {
		}
	}

	printf("logged in!\n");
	return 0;
}
