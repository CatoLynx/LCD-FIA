var umlautReplacementTable = {
    'ä': 'ae',
    'ö': 'oe',
    'ü': 'ue',
    'ß': 'ss',
    'Ä': 'AE',
    'Ö': 'OE',
    'Ü': 'UE',
    'ẞ': 'SS',
};

function addTextAtCursor(el, text) {
    var cursorPosStart = $(el).prop('selectionStart');
    var cursorPosEnd = $(el).prop('selectionEnd');
    var v = $(el).val();
    var textBefore = v.substring(0,  cursorPosStart);
    var textAfter  = v.substring(cursorPosEnd, v.length);
    $(el).val(textBefore + text + textAfter);
    var newCursorPos = cursorPosStart + text.length;
    el.setSelectionRange(newCursorPos, newCursorPos);
}

function replaceUmlautsOnKeyDown(e) {
    if(e.key in umlautReplacementTable) {
        e.preventDefault();
        addTextAtCursor(e.target, umlautReplacementTable[e.key]);
    }
}

function updateNavBar() {
    $('.nav-link').each(function(e) {
        if($(this).prop('href') == location.href) {
            $(this).addClass("active");
        }
    })
}

$(document).ready(updateNavBar);