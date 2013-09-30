function sum(lst) {
  var sum = 0;
  lst.forEach(function (e) {sum += e;});
  return sum;
}

function mean(lst) {
  return sum(lst) / lst.length;
}

function stdev(lst) {
  var m = mean(lst);
  return Math.sqrt(
    sum(lst.map(function (e) {return (e - m) * (e - m);}))
      / (lst.length - 1));
}

module.exports.sum = sum;
module.exports.mean = mean;
module.exports.stdev = stdev;
