function addBackendOpt(config, parser) {
  if (Object.keys(config.BACKENDS).length > 1) {
    parser.add(['--backend', '-b'],
               {
                 type: 'string',
                 default: '',
                 help: 'Which backend to index. Options: ' +
                     Object.keys(config.BACKENDS).join(',')
               });
  }
}
module.exports.addBackendOpt = addBackendOpt;

function selectBackend(config, opts) {
  var backend = config.BACKENDS[opts.options.backend || ''];
  if (!backend) {
    throw new Error("No such backend: " + opts.options.backend);
  }
  return backend;
}

module.exports.selectBackend = selectBackend;
