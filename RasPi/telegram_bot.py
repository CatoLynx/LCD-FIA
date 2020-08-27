import datetime
import io
import json
import logging
import os
import telebot
import time
import traceback

from PIL import Image
from termcolor import colored
from telebot.types import InlineKeyboardMarkup, InlineKeyboardButton

from local_settings import *


JSON_FILE = "dynamic_data/telegram.json"
PROFILE_PIC_DIR = "dynamic_data/telegram_profile_pics"


class FIABot:
    def __init__(self, api_key, admin_cid, admin_mode = False, blocklist = [], max_message_age = 5):
        self.admin_cid = admin_cid
        self.admin_mode = admin_mode
        self.blocklist = blocklist
        self.max_message_age = max_message_age
        
        self.bot = telebot.TeleBot(api_key, threaded=False)
        self.bot.set_update_listener(lambda msgs: self.listener_console_logging(msgs))
        self.bot.me = self.bot.get_me()
        logging.basicConfig()
        self.logger = logging.getLogger("FIABot")
        self.logger.setLevel(logging.INFO)
        
        # Abusing function decorators to enable me to use the code in a class
        self.bot.message_handler(commands=['start'])(lambda msg: self.handle_start(msg))
        self.bot.message_handler(content_types=['text'])(lambda msg: self.handle_text(msg))
    
    def run(self):
        exit = False
        while not exit:
            try:
                self.logger.info("Press Ctrl-C again within one second to terminate the bot")
                time.sleep(1)
                self.logger.info("Starting bot")
                self.bot.polling()
            except KeyboardInterrupt:
                self.logger.info("Goodbye!")
                exit = True
            except:
                traceback.print_exc()
                time.sleep(10)
    
    def filter_message_incoming(self, msg):
        # Discard any non-admin messages if in admin mode
        if self.admin_mode and msg.chat.id != self.admin_cid:
            return False
        # Discard messages from blacklisted users
        if msg.chat.id in self.blocklist:
            return False
        # Discard messages older than the limit
        if (time.time() - msg.date) > (60 * self.max_message_age):
            return False
        return True
    
    def listener_console_logging(self, messages):
        def _readable(msg):
            if msg.content_type == 'text':
                return msg.text
            else:
                if msg.content_type is not None:
                    return colored(msg.content_type.capitalize(), 'blue')
                else:
                    return colored("Unknown Message Type", 'blue')

        for msg in messages:
            filtered = self.filter_message_incoming(msg)
            self.logger.info("%(filtered)s[%(timestamp)s #%(mid)s %(firstname)s %(lastname)s @%(username)s #%(uid)s @ %(groupname)s #%(cid)s] %(text)s" % {
                'filtered': colored("[FILTERED]", 'red') if not filtered else "",
                'timestamp': datetime.datetime.fromtimestamp(msg.date).strftime("%d.%m.%Y %H:%M:%S"),
                'firstname': colored(msg.from_user.first_name, 'green'),
                'lastname': colored(msg.from_user.last_name, 'magenta'),
                'username': colored(msg.from_user.username, 'yellow'),
                'groupname': colored(msg.chat.title, 'red'),
                'mid': colored(str(msg.message_id), 'blue'),
                'uid': colored(str(msg.from_user.id), 'cyan'),
                'cid': colored(str(msg.chat.id), 'cyan'),
                'text': _readable(msg)
            })
    
    def handle_start(self, msg):
        self.bot.reply_to(msg, "Hi! Just send me a text message and I'll display it.")
    
    def handle_text(self, msg):
        profile_pics = self.bot.get_user_profile_photos(msg.chat.id)
        if profile_pics.total_count > 0:
            profile_pic = profile_pics.photos[0]
            for size in profile_pic:
                if size.width >= 48 and size.height >= 48:
                    f_info = self.bot.get_file(size.file_id)
                    f = io.BytesIO()
                    f.write(self.bot.download_file(f_info.file_path))
                    f.seek(0)
                    img = Image.open(f)
                    img = img.resize((48, 48))
                    img.save(os.path.join(PROFILE_PIC_DIR, f"{msg.chat.id}.png"))
                    img.close()
                    f.close()
        
        try:
            with open(JSON_FILE, 'r', encoding='utf-8') as f:
                data = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            data = {}
        data.update({str(msg.chat.id): {
                'type': 'text',
                'text': msg.text,
                'timestamp': msg.date,
                'username': msg.from_user.username,
                'display_name': (msg.from_user.first_name or "") + " " + (msg.from_user.last_name or "")
            }})
        with open(JSON_FILE, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, sort_keys=True)
        self.bot.reply_to(msg, "Your message has been saved!")


"""
MAIN LOOP
"""

def main():
    bot = FIABot(TELEGRAM_API_TOKEN, TELEGRAM_ADMIN_CID)
    bot.run()


if __name__ == "__main__":
    main()
