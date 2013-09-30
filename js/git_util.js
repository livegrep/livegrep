var execFile   = require('child_process').execFile;

module.exports.rev_parse = function (repo, ref, cb) {
  execFile('git', ['rev-parse', ref], {
             cwd: repo
           }, function (err, stdout, stderr) {
             if (err) return cb(err, null);
             var sha1 = stdout.trim();
             cb(null, sha1);
           })
}
