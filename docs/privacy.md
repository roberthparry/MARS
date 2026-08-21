# Privacy

MARS is local software and does not provide a hosted user account, telemetry
service or shared WeatherAPI account. This notice describes the optional
transfer of MARS Lab input involved in a weather lookup in Datetime mode.

## Weather is disabled by default

MARS does not include or provide a shared WeatherAPI key. Weather remains
unavailable unless the person installing MARS Lab creates their own
WeatherAPI.com account and deliberately configures that account's API key. If
no key is configured, MARS makes no request to WeatherAPI.com and the calendar
and astronomical features continue to work locally.

The configured key is stored in the installing user's private
`~/.mars/config/weather.env` file with owner-only permissions. It is read by
the local MARS Lab server and is not sent to the browser. Removing that file or
removing its `MARS_WEATHER_API_KEY` setting disables weather lookups.

## Information sent during a lookup

The browser sends the selected date and observer latitude and longitude to the
local MARS Lab server as part of the Datetime calculation. When weather is
enabled and the selected date is supported, the local server sends the
following information to WeatherAPI.com over HTTPS:

- the API key belonging to the installing user's WeatherAPI account;
- the selected date; and
- the observer latitude and longitude.

WeatherAPI.com will also receive ordinary network request information, such as
the public IP address from which the request reaches its service. Its handling
of that information is governed by the
[WeatherAPI.com privacy policy](https://www.weatherapi.com/privacy.aspx) and
[terms](https://www.weatherapi.com/terms.aspx).

MARS uses the returned data only to display the Weather card. It does not cache
or persist the returned weather response. Weather-specific request logs record
only success or failure and elapsed time, not the selected date, coordinates,
API key or response.

MARS Lab separately stores its ordinary input state, including the selected
Datetime date and observer coordinates, in the private local
`~/.mars/lab/mars_lab_state.json` file so that the input fields can be restored
at the next launch. Its local datetime calculation cache in
`~/.mars/lab/mars_lab_object_store.sqlite3` may also be keyed by those inputs.
This local state is not sent to the MARS project or its contributors.

## Running MARS Lab for other people

The person or organisation operating a MARS Lab server controls that server's
local configuration and stored state. If they make it available to other
people, they must make this notice available to those users and are
responsible for any additional privacy information, lawful basis, security or
retention measures required by their jurisdiction and deployment. They must
not configure another person's WeatherAPI key without that person's
authorisation.
