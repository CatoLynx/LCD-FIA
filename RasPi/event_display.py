import argparse
import datetime
import json
import os
import requests
import socket
import time
import traceback
import tweepy

from PIL import Image, ImageDraw, ImageOps
from mastodon import Mastodon
from pyquery import PyQuery as pq
from urllib.parse import urlparse
from deutschebahn import DBInfoscreen, DS100

from layout_renderer import LayoutRenderer
from fia_control import FIA, FIAEmulator
from display_image import display_image
from utils import TimeoutError, timeout
from db_live_departures import show_departures

from local_settings import *


def static_app(fia, renderer, config):
    pages = config.get('pages', [])
    loop_count = config.get('loop_count', 1)
    for i in range(loop_count):
        for page in pages:
            page_type = page.get('type')

            if page_type == 'layout':
                layout = page.get('layout')
                values = {
                    'placeholders': page.get('values', {})
                }
                if type(layout) is dict:
                    renderer.display(layout, values)
                elif type(layout) is str:
                    with open(layout, 'r', encoding='utf-8') as f:
                        renderer.display(json.load(f), values)
            elif page_type == 'image':
                filename = page.get('file')
                if filename:
                    img = Image.open(filename).convert('L')
                    if img.size != (DISPLAY_WIDTH, DISPLAY_HEIGHT):
                        dest_img = Image.new('L', (DISPLAY_WIDTH, DISPLAY_HEIGHT), 'black')
                        img = img.crop((0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT))
                        dest_img.paste(img, (0, 0))
                        fia.send_image(dest_img)
                    else:
                        fia.send_image(img)
            elif page_type == 'video':
                filename = page.get('file')
                loop_count = page.get('loop_count', 1)
                interval = page.get('interval')
                if filename:
                    display_image(fia, filename, width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT, interval=interval, countdown=False, loop_count=loop_count, output=False)

            time.sleep(page.get('duration', 1))


def webserver_app(fia, renderer, config):
    loop_count = config.get('loop_count', 1)
    data_source = config.get('data_source')
    if data_source is None:
        return
    with open(data_source, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    for i in range(loop_count):
        for text_page in data.get('text_pages', []):
            layout = {
                'width': DISPLAY_WIDTH,
                'height': DISPLAY_HEIGHT,
                'placeholders': [{
                    'name': "text_page_content",
                    'type': "multiline_text",
                    'x': 0,
                    'y': 0,
                    'width': DISPLAY_WIDTH,
                    'height': DISPLAY_HEIGHT,
                    'pad_left': 0,
                    'pad_top': 0,
                    'inverted': text_page.get('inverted', False),
                    'font': text_page.get('font', "10S_DBLCD_custom"),
                    'size': 0,
                    'align': text_page.get('align', 'left'),
                    'spacing': 1,
                    'line_spacing': 1
                }]
            }
            values = {
                'placeholders': {
                    'text_page_content': text_page.get('text', "")
                }
            }
            renderer.display(layout, values)
            time.sleep(text_page.get('duration', 1))


def twitter_app(api, fia, renderer, config):
    def _apply_filter(f, tweets):
        name = f.get('type')
        params = f.get('parameters', {})
        if name == 'no_replies':
            return filter(lambda t: t.in_reply_to_status_id is None, tweets)
        elif name == 'only_replies':
            return filter(lambda t: t.in_reply_to_status_id is not None, tweets)
        elif name == 'no_quotes':
            return filter(lambda t: not t.is_quote_status, tweets)
        elif name == 'only_quotes':
            return filter(lambda t: t.is_quote_status, tweets)
        elif name == 'no_retweets':
            return filter(lambda t: not hasattr(t, 'retweeted_status'), tweets)
        elif name == 'only_retweets':
            return filter(lambda t: hasattr(t, 'retweeted_status'), tweets)
        elif name == 'text_contains':
            return filter(lambda t: params.get('text').lower() in t.text.lower(), tweets)
        elif name == 'text_not_contains':
            return filter(lambda t: params.get('text').lower() not in t.text.lower(), tweets)
        else:
            return tweets
    
    @timeout(10)
    def _get_tweets():
        tweets = []
        for source in tweet_sources:
            source_type = source.get('type')
            params = source.get('parameters', {})
            if source_type == 'search':
                results = api.search(tweet_mode='extended', **params)
            elif source_type == 'user':
                results = api.user_timeline(tweet_mode='extended', **params)
            else:
                results = []
            filters = source.get('filters', [])
            for f in filters:
                results = _apply_filter(f, results)
            tweets.extend(results)
        return tweets
    
    @timeout(10)
    def _prepare_tweet_placeholders():
        tweet_placeholders = []
        size = (48, 48)
        mask = Image.new('L', size, 0)
        draw = ImageDraw.Draw(mask) 
        draw.ellipse((0, 0, size[0]-1, size[1]-1), fill=255)
        for tweet in tweets:
            display_pic = Image.new('L', size, 0)
            profile_pic = Image.open(requests.get(tweet.user.profile_image_url_https, stream=True).raw)
            profile_pic = ImageOps.fit(profile_pic, mask.size, centering=(0.5, 0.5))
            profile_pic.putalpha(mask)
            display_pic.paste(profile_pic, mask)
            
            timestamp = tweet.created_at + datetime.timedelta(hours=2)
            
            tweet_placeholders.append({
                'placeholders': {
                    'tweet_text': tweet.full_text,
                    'tweet_timestamp': timestamp.strftime("%d.%m.%Y %H:%M"),
                    'user_username': "@" + tweet.user.screen_name,
                    'user_display_name': tweet.user.name,
                    'user_profile_pic': display_pic
                }
            })
        return tweet_placeholders
    
    tweet_sources = config.get('tweet_sources', [])
    loop_count = config.get('loop_count', 1)
    num_tweets = config.get('num_tweets', 10)
    tweet_duration = config.get('tweet_duration', 5)
    tweet_layout = config.get('tweet_layout', {})

    # Get tweets
    tweets = _get_tweets()

    # Sort tweets
    tweets = sorted(tweets, key=lambda t: t.created_at, reverse=True)

    # Deduplicate tweets
    unique_tweets = []
    for t in tweets:
        if t.id not in [t.id for t in unique_tweets]:
            unique_tweets.append(t)

    # Limit number of tweets
    tweets = unique_tweets[:num_tweets]
    
    # Prepare placeholder values for tweets
    tweet_placeholders = _prepare_tweet_placeholders()

    # Display tweets
    for i in range(loop_count):
        for i, tweet in enumerate(tweets):
            if type(tweet_layout) is dict:
                renderer.display(tweet_layout, tweet_placeholders[i])
            elif type(tweet_layout) is str:
                with open(tweet_layout, 'r', encoding='utf-8') as f:
                    renderer.display(json.load(f), tweet_placeholders[i])
            time.sleep(tweet_duration)


def telegram_app(fia, renderer, config):
    loop_count = config.get('loop_count', 1)
    data_source = config.get('data_source')
    profile_pics_dir = config.get('profile_pics_dir')
    num_messages = config.get('num_messages', 10)
    message_duration = config.get('message_duration', 5)
    message_layout = config.get('message_layout', {})
    
    if data_source is None:
        return
    with open(data_source, 'r', encoding='utf-8') as f:
        data = json.load(f)
    
    messages = sorted(data.items(), key=lambda item: item[1]['timestamp'], reverse=True)[:num_messages]
    
    # Prepare placeholder values for messages
    message_placeholders = []
    size = (48, 48)
    mask = Image.new('L', size, 0)
    draw = ImageDraw.Draw(mask) 
    draw.ellipse((0, 0, size[0]-1, size[1]-1), fill=255)
    for cid, message in messages:
        try:
            profile_pic = Image.open(os.path.join(profile_pics_dir, f"{cid}.png"))
        except FileNotFoundError:
            display_pic = None
        else:
            display_pic = Image.new('L', size, 0)
            profile_pic = ImageOps.fit(profile_pic, mask.size, centering=(0.5, 0.5))
            profile_pic.putalpha(mask)
            display_pic.paste(profile_pic, mask)
        
        message_placeholders.append({
            'placeholders': {
                'message_text': message['text'],
                'message_timestamp': datetime.datetime.fromtimestamp(message['timestamp']).strftime("%d.%m.%Y %H:%M"),
                'user_username': "@" + message['username'] if message['username'] else None,
                'user_display_name': message['display_name'],
                'user_profile_pic': display_pic
            }
        })
    
    # Display messages
    for i in range(loop_count):
        for i, message in enumerate(messages):
            if type(message_layout) is dict:
                renderer.display(message_layout, message_placeholders[i])
            elif type(message_layout) is str:
                with open(message_layout, 'r', encoding='utf-8') as f:
                    renderer.display(json.load(f), message_placeholders[i])
            time.sleep(message_duration)


def mastodon_app(mastodon, fia, renderer, config):
    @timeout(10)
    def _get_toots():
        toots = []
        for source in toot_sources:
            source_type = source.get('type')
            params = source.get('parameters', {})
            if source_type == 'hashtag':
                results = mastodon.timeline_hashtag(**params)
            else:
                results = []
            toots.extend(results)
        return toots
    
    @timeout(10)
    def _prepare_toot_placeholders():
        toot_placeholders = []
        size = (48, 48)
        mask = Image.new('L', size, 0)
        draw = ImageDraw.Draw(mask)
        draw.ellipse((0, 0, size[0]-1, size[1]-1), fill=255)
        for toot in toots:
            display_pic = Image.new('L', size, 0)
            profile_pic = Image.open(requests.get(toot['account']['avatar'], stream=True).raw)
            profile_pic = ImageOps.fit(profile_pic, mask.size, centering=(0.5, 0.5))
            profile_pic.putalpha(mask)
            display_pic.paste(profile_pic, mask)

            toot_placeholders.append({
                'placeholders': {
                    'toot_text': pq(toot['content']).text(),
                    'toot_timestamp': toot['created_at'].strftime("%d.%m.%Y %H:%M"),
                    'user_username': "@" + toot['account']['acct'],
                    'user_display_name': toot['account']['display_name'],
                    'user_profile_pic': display_pic,
                    'source': urlparse(toot['uri']).netloc
                }
            })
        return toot_placeholders
    
    toot_sources = config.get('toot_sources', [])
    loop_count = config.get('loop_count', 1)
    num_toots = config.get('num_toots', 10)
    toot_duration = config.get('toot_duration', 5)
    toot_layout = config.get('toot_layout', {})

    # Get toots
    toots = _get_toots()

    # Sort toots
    toots = sorted(toots, key=lambda item: item['created_at'], reverse=True)

    # Deduplicate toots
    unique_toots = []
    for t in toots:
        if t.id not in [t.id for t in unique_toots]:
            unique_toots.append(t)

    # Limit number of toots
    toots = unique_toots[:num_toots]

    # Prepare placeholder values for toots
    toot_placeholders = _prepare_toot_placeholders()

    # Display toots
    for i in range(loop_count):
        for i, toot in enumerate(toots):
            if type(toot_layout) is dict:
                renderer.display(toot_layout, toot_placeholders[i])
            elif type(toot_layout) is str:
                with open(toot_layout, 'r', encoding='utf-8') as f:
                    renderer.display(json.load(f), toot_placeholders[i])
            time.sleep(toot_duration)

def weather_app(fia, renderer, config):
    @timeout(10)
    def _get_weather_data():
        return requests.get(f"https://api.openweathermap.org/data/2.5/onecall?lat={latitude}&lon={longitude}&units=metric&appid={OWM_API_TOKEN}").json()
    
    duration = config.get('duration', 10)
    latitude = config.get('latitude')
    longitude = config.get('longitude')
    icon_dir = config.get('icon_dir')
    weather_layout = config.get('weather_layout', {})
    weather_data = _get_weather_data()
    
    weather_1h = weather_data['hourly'][1]
    weather_3h = weather_data['hourly'][3]
    weather_6h = weather_data['hourly'][6]
    weather_1d = weather_data['daily'][1]
    weather_2d = weather_data['daily'][2]
    
    weather_placeholders = {'placeholders': {
        'time_1': "T+1h",
        'time_2': "T+3h",
        'time_3': "T+6h",
        'time_4': "T+1d",
        'time_5': "T+2d",
        'icon_1': os.path.join(icon_dir, weather_1h['weather'][0]['icon'] + ".png"),
        'icon_2': os.path.join(icon_dir, weather_3h['weather'][0]['icon'] + ".png"),
        'icon_3': os.path.join(icon_dir, weather_6h['weather'][0]['icon'] + ".png"),
        'icon_4': os.path.join(icon_dir, weather_1d['weather'][0]['icon'] + ".png"),
        'icon_5': os.path.join(icon_dir, weather_2d['weather'][0]['icon'] + ".png"),
        'temp_1': f"{weather_1h['temp']:.1f} °C",
        'temp_2': f"{weather_3h['temp']:.1f} °C",
        'temp_3': f"{weather_6h['temp']:.1f} °C",
        'temp_4': f"{weather_1d['temp']['day']:.1f} °C",
        'temp_5': f"{weather_2d['temp']['day']:.1f} °C",
        'rain_prob_1': f"{weather_1h['pop']*100:.0f} %",
        'rain_prob_2': f"{weather_3h['pop']*100:.0f} %",
        'rain_prob_3': f"{weather_6h['pop']*100:.0f} %",
        'rain_prob_4': f"{weather_1d['pop']*100:.0f} %",
        'rain_prob_5': f"{weather_2d['pop']*100:.0f} %",
        'wind_speed_1': f"{weather_1h['wind_speed']*3.6:.0f} km/h",
        'wind_speed_2': f"{weather_3h['wind_speed']*3.6:.0f} km/h",
        'wind_speed_3': f"{weather_6h['wind_speed']*3.6:.0f} km/h",
        'wind_speed_4': f"{weather_1d['wind_speed']*3.6:.0f} km/h",
        'wind_speed_5': f"{weather_2d['wind_speed']*3.6:.0f} km/h"
    }}
    if type(weather_layout) is dict:
        renderer.display(weather_layout, weather_placeholders)
    elif type(weather_layout) is str:
        with open(weather_layout, 'r', encoding='utf-8') as f:
            renderer.display(json.load(f), weather_placeholders)
    time.sleep(duration)


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--config', '-c', required=True, type=str)
    parser.add_argument('--font-dir', '-fd', required=True, type=str)
    parser.add_argument('-e', '--emulate', action='store_true', help="Run in emulation mode")
    args = parser.parse_args()
    
    if args.emulate:
        fia = FIAEmulator(width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    else:
        fia = FIA("/dev/ttyAMA1", (3, 0), width=DISPLAY_WIDTH, height=DISPLAY_HEIGHT)
    
    renderer = LayoutRenderer(args.font_dir, fia=fia)
    
    socket.setdefaulttimeout(10.0)
    
    auth = tweepy.OAuthHandler(TWITTER_CONSUMER_KEY, TWITTER_CONSUMER_SECRET)
    auth.set_access_token(TWITTER_ACCESS_TOKEN, TWITTER_ACCESS_TOKEN_SECRET)
    twitter_api = tweepy.API(auth)

    mastodon = Mastodon(api_base_url=MASTODON_API_BASE_URL,
                        client_id=MASTODON_CLIENT_ID,
                        client_secret=MASTODON_CLIENT_SECRET,
                        access_token=MASTODON_ACCESS_TOKEN)
    
    dbi = DBInfoscreen("dbf.finalrewind.org")
    ds100 = DS100()

    with open(args.config, 'r') as f:
        config = json.load(f)
    apps = config.get('apps', [])
    
    print("Starting")

    while True:
        for app in apps:
            try:
                app_type = app.get('type')
                app_config = app.get('config', {})
                if app_type == 'static':
                    static_app(fia, renderer, app_config)
                elif app_type == 'webserver':
                    webserver_app(fia, renderer, app_config)
                elif app_type == 'twitter':
                    twitter_app(twitter_api, fia, renderer, app_config)
                elif app_type == 'telegram':
                    telegram_app(fia, renderer, app_config)
                elif app_type == 'mastodon':
                    mastodon_app(mastodon, fia, renderer, app_config)
                elif app_type == 'weather':
                    weather_app(fia, renderer, app_config)
                elif app_type == 'db_departures':
                    show_departures(dbi, ds100, fia, renderer, app_config, auto_clear_scroll_buf=True)
            except KeyboardInterrupt:
                return
            except:
                traceback.print_exc()
                print("Continuing")
                time.sleep(1)


if __name__ == "__main__":
    main()
