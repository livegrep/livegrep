var $ = require('jquery');
var _ = require('underscore');

function init() {
    $('#repos').selectpicker({
        actionsBox: true,
        selectedTextFormat: 'count > 4',
        countSelectedText: '({0} repositories)',
        noneSelectedText: '(all repositories)',
        liveSearch: true,
        width: '20em'
    });
    $('#repos').on('refreshed.bs.select', function () {
        var headers = $(this).parent().find('.dropdown-header');
        headers.css('cursor', 'pointer');
        headers.on('click', function (event) {
            event.stopPropagation();
            var optgroup = $('#repos optgroup[label="' + $(this).text() + '"]')
            var allSelected = !optgroup.children('option:not(:selected)').length;
            optgroup.children().prop('selected', !allSelected);
            $("#repos").selectpicker('refresh').trigger("change");
        });
    });
    $(window).on('keyup', '.bootstrap-select .bs-searchbox input', function(event) {
        var keycode = (event.keyCode ? event.keyCode : event.which);
        if(keycode == '13'){
            $(this).val("");
            $("#repos").selectpicker('refresh');
        }
    });
    $(window).keyup(function (keyevent) {
        var code = (keyevent.keyCode ? keyevent.keyCode : keyevent.which);
        if (code == 9 && $('.bootstrap-select button:focus').length) {
            $("#repos").selectpicker('toggle');
            $('.bootstrap-select .bs-searchbox input').focus();
        }
    });
}

function escapeHtml(str) {
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function updateOptions(newOptions) {
    // Skip update if the options are the same, to avoid losing selected state.
    var currentOptions = [];
    $('#repos').find('option').each(function(){
        currentOptions.push($(this).attr('value'));
    });
    if (_.isEqual(currentOptions, newOptions)) {
        return;
    }

    newOptions.sort();

    // Build HTML string in one pass instead of creating DOM elements individually.
    var html = '';
    var currentGroup = null;
    for (var i = 0; i < newOptions.length; i++) {
        var parts = newOptions[i].split('/');
        var group = parts.slice(0, parts.length - 1).join('/') + '/';
        var option = parts[parts.length - 1];
        var value = group + option;

        if (group !== '/' && group !== currentGroup) {
            if (currentGroup !== null && currentGroup !== '/') {
                html += '</optgroup>';
            }
            currentGroup = group;
            html += '<optgroup label="' + escapeHtml(group) + '">';
        }

        html += '<option value="' + escapeHtml(value) + '">' + escapeHtml(option) + '</option>';
    }
    if (currentGroup !== null && currentGroup !== '/') {
        html += '</optgroup>';
    }

    $('#repos').html(html);
    $('#repos').selectpicker('refresh');
}

function updateSelected(newSelected) {
    $('#repos').selectpicker('val', newSelected);
}

module.exports = {
    init: init,
    updateOptions: updateOptions,
    updateSelected: updateSelected
}
