var frames = [];
var running = true;

function add_one() {
  var h = new HTMLFactory();
  var iframe = h.iframe({src: "_bench_one.html"});
  frames.push(iframe);
  $('#bench').append(iframe);
  return false;
}

function remove_one() {
  if (frames.length == 0)
    return;
  var frame = frames.pop();
  $(frame).remove()
}

function playpause() {
  var btn = $('#playpause');
  if (running) {
    frames.forEach(function (f) {f.contentWindow.Benchmark.stop();});
    btn.text("start");
  } else {
    frames.forEach(function (f) {f.contentWindow.Benchmark.start();});
    btn.text("stop");
  }
  running = !running;
}

function startup() {
  $('#addone').click(add_one);
  $('#remove').click(remove_one);
  $('#playpause').click(playpause);
  for (var i = 0; i < 4; i++)
    add_one();
}
$(document).ready(startup);

