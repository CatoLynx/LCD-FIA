import argparse
import datetime
import json
import time
import tweepy

from PIL import Image

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

            if 'duration' in page:
                time.sleep(page['duration'])
            else:
                time.sleep(1)


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
            results = api.search(**params)
        elif source_type == 'user':
            results = api.user_timeline(**params)
        else:
            results = []
        filters = source.get('filters', [])
        for f in filters:
            results = _apply_filter(f, results)
        tweets.extend(results)

    # Sort tweets
    tweets = sorted(tweets, key=lambda t: t.created_at)

    # Deduplicate tweets
    unique_tweets = []
    for t in tweets:
        if t.id not in [t.id for t in unique_tweets]:
            unique_tweets.append(t)

    # Limit number of tweets
    tweets = unique_tweets[:num_tweets]

    for i in range(loop_count):
        for tweet in tweets:
            # Display tweet
            values = {
                'placeholders': {
                    'text': tweet.text,
                    'timestamp': tweet.created_at.strftime("%d.%m.%Y %H:%M"),
                    'username': tweet.user.screen_name
                }
            }
            if type(tweet_layout) is dict:
                renderer.display(tweet_layout, values)
            elif type(tweet_layout) is str:
                with open(tweet_layout, 'r', encoding='utf-8') as f:
                    renderer.display(json.load(f), values)
            time.sleep(tweet_duration)


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

    while True:
        for app in apps:
            app_type = app.get('type')
            app_config = app.get('config', {})
            if app_type == 'static':
                static_app(fia, renderer, app_config)
            if app_type == 'twitter':
                twitter_app(twitter_api, fia, renderer, app_config)


if __name__ == "__main__":
    main()
