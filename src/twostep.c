#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"
#include "http.h"
#include "sha1.h"
#include "secret.h"	

PBL_APP_INFO(HTTP_UUID,	"2 Factor Auth v2", "gominosensei",
		2, 1, RESOURCE_ID_IMAGE_MENU_ICON, APP_INFO_STANDARD_APP);

Window window;
TextLayer tokenLayer;
TextLayer countDownLayer;
TextLayer debugLayer;
int timeZoneOffset;  // *msd 7/3/13 

// return seconds since epoch compensating for Pebble's lack of location
// independent GMT
uint32_t get_epoch_seconds(bool remaining) {
	PblTm current_time;
	uint32_t unix_time;
	uint32_t return_time;
	get_time(&current_time);
	
// shamelessly stolen from WhyIsThisOpen's Unix Time source: http://forums.getpebble.com/discussion/4324/watch-face-unix-time
	/* Convert time to seconds since epoch. */
	unix_time = ((0-timeZoneOffset)*3600) + /* time zone offset */  // *msd 7/3/13
		+ current_time.tm_sec /* start with seconds */
		+ current_time.tm_min*60 /* add minutes */
		+ current_time.tm_hour*3600 /* add hours */                                    + current_time.tm_yday*86400 /* add days */
		+ (current_time.tm_year-70)*31536000 /* add years since 1970 */                + ((current_time.tm_year-69)/4)*86400 /* add a day after leap years, starting in 1973 */                                                                       - ((current_time.tm_year-1)/100)*86400 /* remove a leap day every 100 years, starting in 2001 */                                                               + ((current_time.tm_year+299)/400)*86400; /* add a leap day back every 400 years, starting in 2001*/
	if (remaining) {
		return_time = 30 - (unix_time % 30);
	}
	else {
		return_time = unix_time / 30;
	}
	return return_time;
}

void set_colors(TextLayer* layer, bool inverted) {
	(void)layer;
	if (inverted)
	{
		text_layer_set_background_color(layer, GColorWhite);
		text_layer_set_text_color(layer, GColorBlack);
	}
	else
	{
		text_layer_set_background_color(layer, GColorBlack);
		text_layer_set_text_color(layer, GColorWhite);
	}
}

void update_countDown(AppContextRef ctx) {
	(void)ctx;
	static char displayTimeRemaining[] = "19";
	
	uint32_t remaining_time = get_epoch_seconds(true);	
	displayTimeRemaining[0]=0x30 + remaining_time / 10;
	displayTimeRemaining[1]=0x30 + remaining_time % 10;

	text_layer_set_text(&countDownLayer, displayTimeRemaining);
	
	// Alert if there's not much time left
	if (remaining_time < 6)
	{
		if (remaining_time % 2 == 0)
		{
			set_colors(&countDownLayer, false);
		}
		else
		{
			set_colors(&countDownLayer, true);
		}
	}
	else {
		set_colors(&countDownLayer, false);
	}
}

void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t) {

	(void)t;
	(void)ctx;

	static char tokenText[] = "RYRYRY"; // Needs to be static because it's used by the system later.

	sha1nfo s;
	uint8_t ofs;
	uint32_t otp;
	int i;
	uint32_t unix_time;
	char sha1_time[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	// TOTP uses seconds since epoch in the upper half of an 8 byte payload
	// TOTP is HOTP with a time based payload
	// HOTP is HMAC with a truncation function to get a short decimal key
	unix_time = get_epoch_seconds(false);
	sha1_time[4] = (unix_time >> 24) & 0xFF;
	sha1_time[5] = (unix_time >> 16) & 0xFF;
	sha1_time[6] = (unix_time >> 8) & 0xFF;
	sha1_time[7] = unix_time & 0xFF;

	// First get the HMAC hash of the time payload with the shared key
	sha1_initHmac(&s, sha1_key, SECRET_SIZE);
	sha1_write(&s, sha1_time, 8);
	sha1_resultHmac(&s);
	
	// Then do the HOTP truncation.  HOTP pulls its result from a 31-bit byte
	// aligned window in the HMAC result, then lops off digits to the left
	// over 6 digits.
	ofs=s.state.b[SHA1_SIZE-1] & 0xf;
	otp = 0;
	otp = ((s.state.b[ofs] & 0x7f) << 24) |
		((s.state.b[ofs + 1] & 0xff) << 16) |
		((s.state.b[ofs + 2] & 0xff) << 8) |
		(s.state.b[ofs + 3] & 0xff);
	otp %= DIGITS_TRUNCATE;
	
	// Convert result into a string.  Sure wish we had working snprintf...
	for(i = 0; i < 6; i++) {
		tokenText[5-i] = 0x30 + (otp % 10);
		otp /= 10;
	}
	tokenText[6]=0;
	
	text_layer_set_text(&tokenLayer, tokenText);
	// Update the countdown timer
	update_countDown(ctx);
}

//*msd++ 7/2/13
void have_time(int32_t dst_offset, bool is_dst, uint32_t unixtime, const char* tz_name, void* context) {
	timeZoneOffset = dst_offset / 3600;
}

//*msd++ 7/3/13
void set_tz_offset(AppContextRef ctx) {
	timeZoneOffset = CDT;  // set a reasonable default in case http fails
	http_set_app_id(34525634);
	http_register_callbacks((HTTPCallbacks){
		.time=have_time,
	}, NULL);
	http_time_request();
}

void handle_init(AppContextRef ctx) {
	(void)ctx;
	int vertOffset = 15;
	//int labelTokenTop=0;
	//int labelHeight=35;
	int tokenTop = 25;
	int tokenHeight = 43;
	int countTop = tokenTop + tokenHeight + vertOffset;
	int countHeight = tokenHeight;
	int countWidth = 65;
	int countLeft = (144 - countWidth) / 2;
	/*
	int debugTop = countTop + countHeight + 5;
	int debugHeight = 30;
	*/

	// max height 168
	// max width 144

	window_init(&window, "tstep");
	window_stack_push(&window, true /* Animated */);
	window_set_background_color(&window, GColorBlack);

	// Label for the token
	/*
	text_layer_init(&labelTokenLayer, GRect(0, labelTokenTop, 144, labelHeight));
	text_layer_set_text_color(&labelTokenLayer, GColorBlack);
	text_layer_set_background_color(&labelTokenLayer, GColorWhite);
	text_layer_set_text_alignment(&labelTokenLayer, GTextAlignmentCenter);
	text_layer_set_font(&labelTokenLayer, fonts_get_system_font(FONT_KEY_GOTHAM_18_LIGHT_SUBSET));
	layer_add_child(&window.layer, &labelTokenLayer.layer);
	text_layer_set_text(&labelTokenLayer, "foo");
	*/
	
	// Init the text layer for the token
	text_layer_init(&tokenLayer, GRect(0, tokenTop, 144, tokenHeight));
	set_colors(&tokenLayer, false);
	//text_layer_set_text_color(&tokenLayer, GColorWhite);
	//text_layer_set_background_color(&tokenLayer, GColorClear);
	text_layer_set_text_alignment(&tokenLayer, GTextAlignmentCenter);
	text_layer_set_font(&tokenLayer, fonts_get_system_font(FONT_KEY_GOTHAM_34_MEDIUM_NUMBERS));
	layer_add_child(&window.layer, &tokenLayer.layer);

	// Init the text layer for the countdown timer
	text_layer_init(&countDownLayer, GRect(countLeft, countTop, countWidth, countHeight));
	set_colors(&countDownLayer, false);
	//text_layer_set_text_color(&countDownLayer, GColorWhite);
	//text_layer_set_background_color(&countDownLayer, GColorClear);
	text_layer_set_text_alignment(&countDownLayer, GTextAlignmentCenter);
	text_layer_set_font(&countDownLayer, fonts_get_system_font(FONT_KEY_GOTHAM_34_MEDIUM_NUMBERS));
	layer_add_child(&window.layer, &countDownLayer.layer);

	// Init the layer to display debugging text  *msd++ 7/2/13
	/*
	text_layer_init(&debugLayer, GRect(0, debugTop, 144, debugHeight));
	set_colors(&debugLayer, false);
	text_layer_set_text_alignment(&debugLayer, GTextAlignmentCenter);
	text_layer_set_font(&debugLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(&window.layer, &debugLayer.layer);
	static char foo[] = "bar";
	text_layer_set_text(&debugLayer, foo);
	*/

	// Find the time zone offset - used to calculate the TOTP  *msd+1 7/2/13
	set_tz_offset(ctx);
	
	handle_second_tick(ctx, NULL);
}

void pbl_main(void *params) {
	PebbleAppHandlers handlers = {
		.init_handler = &handle_init,
		.tick_info = {
			.tick_handler = &handle_second_tick,
			.tick_units = SECOND_UNIT
		},
		.messaging_info = {
			.buffer_sizes = {
				.inbound = 256,
				.outbound = 256,
			}
		}
	};
	app_event_loop(params, &handlers);
}
