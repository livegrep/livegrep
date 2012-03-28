var Feedback = (
  function() {
    return {
      div: null,
      link: null,
      submit: null,
      visible: false,
      onload: function() {
        Feedback.div = $('#feedback');
        Feedback.link = $('#feedbacklink');
        Feedback.submit = $('#feedback button');
        Feedback.spinner = $('#feedback img.spinner');

        Feedback.visible = false;
        Feedback.link.click(Feedback.toggle);
        Feedback.link.attr('href', '#');
        Feedback.submit.click(Feedback.send);
      },
      toggle: function() {
        Feedback.visible = !Feedback.visible;
        if (Feedback.visible) {
          var pos = Feedback.link.position();
          pos.top += Feedback.link.height() + 5;
          pos.left += 5;
          Feedback.div.css(pos);
          Feedback.div.show();
        } else {
          Feedback.div.hide();
        }
        return false;
      },
      send: function() {
        var data = {};
        try {
          data.session = Codesearch.socket.socket.sessionid;
        } catch(e) {
          data.session = null;
        }

        data.text  = $('#feedback textarea').val();
        data.email = $('#feedback input.email').val();

        var result = $('#feedback span.result');
        result.text('');

        Feedback.spinner.show();

        $.post('/feedback',
               {data: JSON.stringify(data)},
               function() {
                 result.text('Sent!')
                 Feedback.spinner.hide();
                 $('#feedback form')[0].reset()
               }).error(
                 function() {
                   result.text('Unable to submit feedback.')
                   Feedback.spinner.hide();
                 });

        return false;
      }
    };
  })();
$(document).ready(Feedback.onload);
