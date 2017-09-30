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

function updateOptions(newOptions) {
    // Skip update if the options are the same, to avoid losing selected state.
    var currentOptions = [];
    $('#repos').find('option').each(function(){
        currentOptions.push($(this).attr('value'));
    });
    if (_.isEqual(currentOptions, newOptions)) {
        return;
    }

    $('#repos').empty();
    for (var i = 0; i < newOptions.length; i++) {
        var option = newOptions[i];
        $('#repos').append($('<option>').attr('value', option).text(option));
    }
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
