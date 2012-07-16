#include <stdio.h>
#include <libspotify/api.h>

extern const uint8_t g_appkey[];
extern const size_t g_appkey_size;

// TODO: get this working?
static sp_session_callbacks session_callbacks = {
	.logged_in = NULL,
	.notify_main_thread = NULL,
	.music_delivery = NULL,
	.metadata_updated = NULL,
	.play_token_lost = NULL,
	.log_message = NULL,
	.end_of_track = NULL
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
	sp_session *sp;
	sp_error err;
	const char *username = NULL;
	const char *password = NULL;
	return 0;
}
