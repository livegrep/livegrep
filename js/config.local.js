var path = require('path');

/* module.exports = {
  SLOW_THRESHOLD: 300,
  BACKENDS: {livegrep: {
    host: "localhost",
    port: 0xC5EA,
    connections: 4,
    index: path.join(__dirname, "livegrep.idx"),
    pretty_name: "livegrep",
    repos: [
      {
        path: path.join(__dirname, ".."),
        name: "Livegrep",
        refs: ["HEAD"],
        github: "nelhage/codesearch",
      },
    ],
    sort: [],
  },
  random: {
    host: "localhost",
    port: 0xC5EB,
    connections: 4,
    index: path.join(__dirname, "random.idx"),
    pretty_name: "Random Crap",
    repos: [
      {
        path: '/home/nelhage/code/mosh',
        name: "mosh",
        refs: ["HEAD"],
        github: "keithw/mosh",
      },
      {
        path: '/home/nelhage/code/meteor',
        name: "meteor",
        refs: ["HEAD"],
        github: "meteor/meteor",
      },
    ],
    sort: [],
  }}
};
*/

module.exports = {
  BACKENDS: {'avr-libc': {
    host: "localhost",
    port: 0xC5EA,
    connections: 4,
    index: path.join(__dirname, "avr-libc.idx"),
    pretty_name: "AVR libc",
    repos: [
      {
        path: '/home/nelhage/src/avr-libc/',
        name: "AVR libc",
        refs: ["HEAD"],
        github: "",
      },
    ],
    sort: [],
  }}
};
