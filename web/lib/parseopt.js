var parseopt = require('parseopt');

var OptionParser = function(opts) {
  var self = this;
  if (opts === undefined)
    opts = {};
  opts.options = opts.options || [];
  opts.options.push({
      names: ['--help', '-h'],
      type: 'flag',
      help: 'Show this help message.',
      onOption: function (value) {
        self.usage();
        process.exit(0);
      }
  });
  parseopt.OptionParser.call(this, opts);
}

OptionParser.prototype = new parseopt.OptionParser();

module.exports.OptionParser = OptionParser;
