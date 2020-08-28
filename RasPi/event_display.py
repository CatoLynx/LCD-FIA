import argparse
import datetime
import json
import os
import requests
import time
import traceback
import tweepy

from PIL import Image, ImageDraw, ImageOps

from layout_renderer import LayoutRenderer
from fia_control import FIA, FIAEmulator
from display_image import display_image

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

    tweet_sources = config.get('tweet_sources', [])
    loop_count = config.get('loop_count', 1)
    num_tweets = config.get('num_tweets', 10)
    tweet_duration = config.get('tweet_duration', 5)
    tweet_layout = config.get('tweet_layout', {})

    # Get tweets
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

    auth = tweepy.OAuthHandler(TWITTER_CONSUMER_KEY, TWITTER_CONSUMER_SECRET)
    auth.set_access_token(TWITTER_ACCESS_TOKEN, TWITTER_ACCESS_TOKEN_SECRET)
    twitter_api = tweepy.API(auth)

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
            except KeyboardInterrupt:
                return
            except:
                traceback.print_exc()
                print("Continuing")
                time.sleep(1)


if __name__ == "__main__":
    main()
