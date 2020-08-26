import json
import platform
import time

from flask import Flask, request, render_template, redirect, send_from_directory
from flask_wtf import FlaskForm
from wtforms import BooleanField, StringField, SubmitField, FormField, FieldList, IntegerField, SelectField
from wtforms.widgets import TextArea
from wtforms.widgets.html5 import NumberInput
from wtforms.validators import NumberRange

from pprint import pprint

NODE_NAME = platform.node()

app = Flask(__name__, template_folder="webserver/templates")
app.config['WTF_CSRF_ENABLED'] = False


JSON_FILE = "dynamic_data/webserver.json"
NUM_LINES = 10
ALIGN_CHOICES = [('left', "Left"), ('center', "Center"), ('right', "Right")]
FONT_CHOICES = [('7_DBLCD', "DBLCD 7px"), ('10_DBLCD_custom', "DBLCD 10px"), ('10S_DBLCD_custom', "DBLCD 10px bold"), ('12_DBLCD', "DBLCD 12px"), ('14_DBLCD', "DBLCD 14px"), ('14S_DBLCD', "DBLCD 14px bold")]


class TextPageForm(FlaskForm):
    duration = IntegerField("Duration", default=10,
        validators=[NumberRange(1, 60)],
        widget=NumberInput(min=1, max=60),
        render_kw={'class': 'text-page-duration rounded mb-3'})
    text = StringField("Text", widget=TextArea(),
        render_kw={'class': 'text-page-content fullwidth page-contents rounded', 'rows': NUM_LINES})
    align = SelectField("Align",
        choices=ALIGN_CHOICES,
        render_kw={'class': 'text-page-align rounded mb-3'})
    font = SelectField("Font",
        choices=FONT_CHOICES,
        render_kw={'class': 'text-page-font rounded mb-3'})
    inverted = BooleanField("Inverted",
        render_kw={'class': 'text-page-inverted rounded mb-3'})


class TextPagesForm(FlaskForm):
    text_pages = FieldList(FormField(TextPageForm), min_entries=1)
    submit = SubmitField("Update",
        render_kw={'class': 'fullwidth'})


@app.route("/", methods=['GET', 'POST'])
def root():
    return redirect("/text-pages", code=302)
    
@app.route("/text-pages", methods=['GET', 'POST'])
def text_pages():
    try:
        with open(JSON_FILE, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        data = {}
    form = TextPagesForm(text_pages=data.get('text_pages', []))
    if form.validate_on_submit():
        data.update({'text_pages': form.data['text_pages']})
        with open(JSON_FILE, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, sort_keys=True)
    return render_template("text_pages.html", form=form, node_name=NODE_NAME)

@app.route('/css/<path:path>')
def css(path):
    return send_from_directory('webserver/css', path)

@app.route('/js/<path:path>')
def js(path):
    return send_from_directory('webserver/js', path)

@app.route('/img/<path:path>')
def img(path):
    return send_from_directory('webserver/img', path)

@app.route('/favicon.ico')
def favicon():
    return send_from_directory('webserver/img', "favicon.ico")
