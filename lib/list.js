function shuffle(lst) {
  for (var i = lst.length - 1; i >= 0; i--) {
    var j = Math.floor(Math.random() * (i+1));
    var tmp = lst[i];
    lst[i] = lst[j];
    lst[j] = tmp;
  }
  return lst;
}

module.exports.shuffle = shuffle;
